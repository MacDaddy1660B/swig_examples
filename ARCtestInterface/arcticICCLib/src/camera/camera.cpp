#include <cstdint>
#include <string>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "ArcDefs.h"
#include "CArcFitsFile.h"
#include "CArcDeinterlace.h"

#include "arcticICC/camera.h"


///I think I can create a data structure that holds the times from the data here.  Then return it back up the chain to the python wrapper  - Shane


namespace {
    int const XBinnedPrescanPerAmp = 2;     // width of x prescan region, in binned pixels (if it can be fixed)
                                        // this is the remaining prescan after removing what we can
                                        // using the SXY (skip X,Y) command
    int const YQuadBorder = 2;          // border between Y amp images when using quad readout

    std::string const TimingBoardFileName = "/home/arctic/leach/tim.lod";

    // see ArcDefs.h and CommandDescription.pdf (you'll need both)
    // note that the two do not agree for any other options and we don't need them anyway
    std::map<arcticICC::ReadoutAmps, int> ReadoutAmpsCmdValueMap {
        {arcticICC::ReadoutAmps::LL,   arc::AMP_0},
        {arcticICC::ReadoutAmps::LR,   arc::AMP_1},
        {arcticICC::ReadoutAmps::UR,   arc::AMP_2},
        {arcticICC::ReadoutAmps::UL,   arc::AMP_3},
        {arcticICC::ReadoutAmps::Quad, arc::AMP_ALL}
    };

    std::map<arcticICC::ReadoutAmps, arc::gen3::dlace::e_Alg> ReadoutAmpsDeinterlaceAlgorithmMap {
        {arcticICC::ReadoutAmps::LL,    arc::gen3::dlace::e_Alg::NONE},
        {arcticICC::ReadoutAmps::LR,    arc::gen3::dlace::e_Alg::NONE},
        {arcticICC::ReadoutAmps::UR,    arc::gen3::dlace::e_Alg::NONE},
        {arcticICC::ReadoutAmps::UL,    arc::gen3::dlace::e_Alg::NONE},
        {arcticICC::ReadoutAmps::Quad,  arc::gen3::dlace::e_Alg::QUAD_CCD}
    };
    
    std::map<int, int> ColBinXSkipMap_One {
        {1, 4},
        {2, 4},
        {3, 3},
        {4, 4}
    };

    std::map<int, int> ColBinXSkipMap_Quad {
        {1, 4},
        {2, 4},
        {3, 3},
        {4, 4}
    };

    // SPS <rate> command: set readout rate
    // The following was taken from Owl's SelectableReadoutSpeedCC.bsh
    int const SPS = 0x535053;   // ASCII for SPS
    std::map<arcticICC::ReadoutRate, int> ReadoutRateCmdValueMap = {
        {arcticICC::ReadoutRate::Slow,   0x534C57}, // ASCII for SLW
        {arcticICC::ReadoutRate::Medium, 0x4D4544}, // ASCII for MED
        {arcticICC::ReadoutRate::Fast,   0x465354}, // ASCII for FST
    };

    // SXY <cols> <rows> command: set initial number of rows and columns to skip
    // Use as needed when binning in quad mode to make sure the last row and column of data is purely data,
    // rather than a mix of data and overscan; otherwise use 0,0
    int const SXY = 0x535859;   // ASCII for  SXY

    /**
    Format a Leach controller command as a string of three capital letters

    If the int cannot be formatted that way then return it in hex format
    */
    std::string formatCmd(int cmd) {
        if ((cmd > 0) && (cmd <= 0xFFFFFF)) {
            std::ostringstream os;
            for (int i = 2; i >= 0; --i) {
                char c = (cmd >> (i*8)) & 0xFF;
                if ((c < 'A') || (c > 'Z')) {
                    goto useHex;
                }
                os << c;
            }
            return os.str();
        }

        useHex:
        std::ostringstream hexos;
        hexos << std::hex << "0x" << cmd;
        return hexos.str();
    }

    /**
    Return the time interval, in fractional seconds, between two chrono steady_clock times

    based on http://www.cplusplus.com/reference/chrono/steady_clock/
    but it's not clear the cast is required; it may suffice to specify duration<double>
    */
    double elapsedSec(std::chrono::steady_clock::time_point const &tBeg, std::chrono::steady_clock::time_point const &tEnd) {
        return std::chrono::duration_cast<std::chrono::duration<double>>(tEnd - tBeg).count();
    }
}

namespace arcticICC {
    CameraConfig::CameraConfig() :
        readoutAmps(ReadoutAmps::Quad),
        readoutRate(ReadoutRate::Medium),
        binFacCol(2),
        binFacRow(2),
        winStartCol(0),
        winStartRow(0),
        winWidth(CCDWidth/2),
        winHeight(CCDHeight/2)
    {}

    std::ostream &operator<<(std::ostream &os, CameraConfig const &config) {
        os << "CameraConfig(readoutAmps=" << ReadoutAmpsNameMap.find(config.readoutAmps)->second
            << ", readoutRate=" << ReadoutRateNameMap.find(config.readoutRate)->second
            << ", binFacCol=" << config.binFacCol
            << ", binFacRow=" << config.binFacRow
            << ", winStartCol=" << config.winStartCol
            << ", winStartRow=" << config.winStartRow
            << ", winWidth=" << config.winWidth
            << ", winHeight=" << config.winHeight
            << ")";
	return os;
    }

    void CameraConfig::assertValid() const {
        std::cout << "assertValid: isFullWindow=" << isFullWindow() << "; canWindow=" << canWindow() << std::endl;
        if (!isFullWindow() && !canWindow()) {
            std::ostringstream os;
            os << "cannot window unless reading from a single amplifier; readoutAmps="
                << ReadoutAmpsNameMap.find(readoutAmps)->second;
            throw std::runtime_error(os.str());
        }
        if (binFacCol < 1 or binFacCol > MaxBinFactor) {
            std::ostringstream os;
            os << "binFacCol=" << binFacCol << " < 1 or > " << MaxBinFactor;
            throw std::runtime_error(os.str());
        }
        if (binFacRow < 1 or binFacRow > MaxBinFactor) {
            std::ostringstream os;
            os << "binFacRow=" << binFacRow << " < 1 or > " << MaxBinFactor;
            throw std::runtime_error(os.str());
        }

        int const binnedCCDWidth = computeBinnedWidth(CCDWidth);
        int const binnedCCDHeight = computeBinnedHeight(CCDHeight);
        if ((winStartCol < 0) || (winStartCol >= binnedCCDWidth)) {
            std::ostringstream os;
            os << "winStartCol=" << winStartCol << " < 0 or >= " << binnedCCDWidth;
            throw std::runtime_error(os.str());
        }
        if ((winStartRow < 0) || (winStartRow >= binnedCCDHeight)) {
            std::ostringstream os;
            os << "winStartRow=" << winStartRow << " < 0 or >= " << binnedCCDHeight;
            throw std::runtime_error(os.str());
        }
        if ((winWidth < 1) || (winWidth > binnedCCDWidth - winStartCol)) {
            std::ostringstream os;
            os << "winWidth=" << winWidth << " < 1 or > " << binnedCCDWidth - winStartCol;
            throw std::runtime_error(os.str());
        }
        if ((winHeight < 1) || (winHeight > binnedCCDHeight - winStartRow)) {
            std::ostringstream os;
            os << "winHeight=" << winHeight << " < 1 or > " << binnedCCDHeight - winStartRow;
            throw std::runtime_error(os.str());
        }

        // if the following test fails we have mis-set some parameter or are mis-computing getBinnedWidth or getBinnedHeight
        if (getNumAmps() > 1) {
            // the number of binned rows and columns must be even
            if ((getBinnedWidth() % 2 != 0) || (getBinnedHeight() % 2 != 0)) {
                std::ostringstream os;
                os << "Bug: reading from multiple amplifiers, so the binned width=" << getBinnedWidth()
                    << " and height=" << getBinnedHeight() << " must both be even";
                throw std::runtime_error(os.str());
            }
        }
    }

    int CameraConfig::getBinnedWidth() const {
        // Warning: if you change this code, also update getMaxWidth
        int xPrescan = XBinnedPrescanPerAmp * ((getNumAmps() > 1) ? 2 : 1);
        return winWidth + xPrescan + computeBinnedWidth(XOverscan);
    }

    int CameraConfig::getMaxWidth() {
        return CCDWidth + (2 * XBinnedPrescanPerAmp) + XOverscan;
    }

    int CameraConfig::getBinnedHeight() const {
        // Warning: if you change this code, also update getMaxHeight
        return winHeight + ((getNumAmps() > 1) ? YQuadBorder : 0);
    }

    int CameraConfig::getMaxHeight() {
        return CCDHeight + YQuadBorder;
    }


    Camera::Camera() :
        _config(),
        _expName(),
        _expType(ExposureType::Object),
        _cmdExpSec(-1),
        _estExpSec(-1),
        _segmentExpSec(-1),
        _segmentStartTime(),
        _segmentStartValid(false),
        _device()
    {
        int const fullWidth = CameraConfig::getMaxWidth();
        int const fullHeight = CameraConfig::getMaxHeight();
        int const numBytes = fullWidth * fullHeight * sizeof(uint16_t);
#ifdef _PCIe
	std::cout << "arc::device::CArcPCIe::findDevices()\n";
        arc::gen3::CArcPCIe::findDevices();
#elif _PCI
        std::cout << "arc::device::CArcPCI::findDevices()\n";
        arc::gen3::CArcPCI::findDevices();
#endif

        std::cout << "_device.deviceCount()=" << _device.deviceCount() << std::endl;
        if (_device.deviceCount() < 1) {
            throw std::runtime_error("no Leach controller found");
        }
        std::cout << "_device.open(0, " << numBytes << ")\n";
        _device.open(0, numBytes);
        std::cout << "_device.isControllerConnected()" << std::endl;
        if (!_device.isControllerConnected()) {
            throw std::runtime_error("Controller is disconnected or powered off");
        }
        std::cout << "_device.setupController(true, true, true, " << fullHeight << ", " << fullWidth << ", \"" << TimingBoardFileName.c_str() << "\")\n";
        _device.setupController(
            true,       // reset?
            true,       // send TDLS to the PCIe board and any board whose .lod file is not NULL?
            true,       // power on the controller?
            fullHeight, // image height
            fullWidth,  // image width
            TimingBoardFileName.c_str() // timing board file to load
        );

        // set default configuration
        setConfig(_config);
        _setIdle();
    }

    Camera::~Camera() {
        if (_device.isReadout()) {
            // abort readout, else the board will keep reading out, which ties it up
            _device.stopExposure();
        }
        _device.close();
    }

    void Camera::startExposure(double expTime, ExposureType expType, std::string const &name) {
        _assertIdle();
        if (expTime < 0) {
            std::ostringstream os;
            os << "exposure time=" << expTime << " must be non-negative";
            throw std::runtime_error(os.str());
        }
        if ((expType == ExposureType::Bias) && (expTime > 0)) {
            std::ostringstream os;
            os << "exposure time=" << expTime << " must be zero if expType=Bias";
            throw std::runtime_error(os.str());
        }

        // clear common buffer, so we know when new data arrives
        _clearBuffer();

        bool openShutter = expType > ExposureType::Dark;
        std::cout << "_device.setOpenShutter(" << openShutter << ")\n";
        _device.setOpenShutter(openShutter);

        uint32_t expTimeMS = uint32_t(expTime*1000.0);
        std::ostringstream os;
        os << "Set exposure time to " << expTimeMS << " ms";
        runCommand(os.str(), arc::TIM_ID, arc::SET, expTimeMS);

        runCommand("start exposure", arc::TIM_ID, arc::SEX);
        _expName = name;
        _expType = expType;
        _cmdExpSec = expTime;
        _estExpSec = -1;
        _segmentExpSec = _cmdExpSec;
        _segmentStartTime = std::chrono::steady_clock::now();
        _segmentStartValid = true;
    }

    void Camera::pauseExposure() {
        if (getExposureState().state != StateEnum::Exposing) {
            throw std::runtime_error("no exposure to pause");
        }
        runCommand("pause exposure", arc::TIM_ID, arc::PEX);
        // decrease _segmentExpSec by the duration of the exposure segment just ended
        double segmentSec = elapsedSec(_segmentStartTime, std::chrono::steady_clock::now());
        _segmentExpSec -= segmentSec;
        _segmentStartValid = false;    // indicates that _segmentStartTime is invalid
    }

    void Camera::resumeExposure() {
        if (getExposureState().state != StateEnum::Paused) {
            throw std::runtime_error("no paused exposure to resume");
        }
        runCommand("resume exposure", arc::TIM_ID, arc::REX);
        _segmentStartTime = std::chrono::steady_clock::now();
        _segmentStartValid = true;
    }

    void Camera::abortExposure() {
        if (!isBusy()) {
            throw std::runtime_error("no exposure to abort");
        }
        std::cout << "_device.stopExposure()\n";
        _device.stopExposure();
        _setIdle();
    }

    void Camera::stopExposure() {
        auto expState = getExposureState();
        if (!expState.isBusy()) {
            throw std::runtime_error("no exposure to stop");
        }
        if (expState.state == StateEnum::Reading || expState.remTime < 0.1) {
            // if reading out or nearly ready to read out then it's too late to stop; let the exposure end normally
            return;
        } else if (expState.state == StateEnum::Paused) {
            // stop a paused exposure; _segmentExpSec contains the remaining time for a full exposure
            _estExpSec = std::max(0.0, _cmdExpSec - _segmentExpSec);
        } else if (expState.state == StateEnum::Exposing) {
            // stop an active exposure
            double segmentDuration = elapsedSec(_segmentStartTime, std::chrono::steady_clock::now());
            double missingTime = _segmentExpSec - segmentDuration;
            _estExpSec = std::max(0.0, _cmdExpSec - missingTime);
        }
        runCommand("stop exposure", arc::TIM_ID, arc::SET, 0);
    }

    ExposureState Camera::getExposureState() {
        if (_cmdExpSec < 0) {
            return ExposureState(StateEnum::Idle);
        } else if (!_segmentStartValid) {
            return ExposureState(StateEnum::Paused);
        } else if (_device.isReadout()) {
            _bufferCleared = false; // when no longer reading, makes sure the next state is ImageRead, not Exposing
            int totPix = _config.getBinnedWidth() * _config.getBinnedHeight();
            int numPixRead = _device.getPixelCount();
            int numPixRemaining = std::max(totPix - numPixRead, 0);
            double fullReadTime = _estimateReadTime(totPix);
            double remReadTime = _estimateReadTime(numPixRemaining);
            if (_estExpSec < 0) {
                // exposure finished normally (instead of being stopped early); assume it was the right length
                _estExpSec = _cmdExpSec;
            }
            return ExposureState(StateEnum::Reading, fullReadTime, remReadTime);
        } else if (_bufferCleared && (_device.commonBufferVA())[0] == (uint8_t)0) {
            // the && above helps in case getExposureState was not called often enough to clear _bufferCleared
            double segmentRemTime = _segmentExpSec - elapsedSec(_segmentStartTime, std::chrono::steady_clock::now());
            return ExposureState(StateEnum::Exposing, _cmdExpSec, segmentRemTime);
        } else {
            return ExposureState(StateEnum::ImageRead);
        }
    }

    void Camera::setConfig(CameraConfig const &config) {
        std::cout << "setConfig(" << config << ")\n";
        config.assertValid();
        _assertIdle();

        runCommand("set col bin factor",  arc::TIM_ID, arc::WRM, ( arc::Y_MEM | 0x5 ), config.binFacCol);

        runCommand("set row bin factor",  arc::TIM_ID, arc::WRM, ( arc::Y_MEM | 0x6 ), config.binFacRow);

        if (config.isFullWindow()) {
            runCommand("set full window", arc::TIM_ID, arc::SSS, 0, 0, 0);
        } else {
            // set subarray size; warning: this only works when reading from one amplifier
            // arguments are:
            // - arg1 is the bias region width (in pixels)
            // - arg2 is the subarray width (in pixels)
            // - arg3 is the subarray height (in pixels)
            uint32_t const xExtraPix = config.getBinnedWidth() - config.winWidth;
            runCommand("set window size", arc::TIM_ID, arc::SSS, xExtraPix, config.winWidth, config.winHeight);

            // set subarray starting-point; warning: this only works when reading from one amplifier
            // SSP arguments are as follows (indexed from 0,0, unbinned pixels)
            // - arg1 is the subarray Y position. This is the number of rows (in pixels) to the lower left corner of the desired subarray region.
            // - arg2 is the subarray X position. This is the number of columns (in pixels) to the lower left corner of the desired subarray region.
            // - arg3 is the bias region offset. This is the number of columns (in pixels) to the left edge of the desired bias region.
            uint32_t const windowEndCol = config.winStartCol + config.winWidth;
            uint32_t const afterDataGap = 5 + config.computeBinnedWidth(CCDWidth) - windowEndCol; // 5 skips some odd gunk
            runCommand("set window position", arc::TIM_ID, arc::SSP, config.winStartRow, config.winStartCol, afterDataGap);
        }

        uint32_t readoutAmpsCmdValue = ReadoutAmpsCmdValueMap.find(config.readoutAmps)->second;
        runCommand("set readoutAmps", arc::TIM_ID, arc::SOS, readoutAmpsCmdValue, arc::DON);

        uint32_t readoutRateCmdValue = ReadoutRateCmdValueMap.find(config.readoutRate)->second;
        runCommand("set readout rate", arc::TIM_ID, SPS, readoutRateCmdValue, arc::DON);

        if (config.readoutAmps == ReadoutAmps::Quad) {
            uint32_t xSkip = ColBinXSkipMap_Quad.find(config.binFacCol)->second;
            uint32_t ySkip = config.binFacRow == 3 ? 1 : 0;
            runCommand("set xy skip for all amps", arc::TIM_ID, SXY, xSkip, ySkip);
        } else {
            uint32_t xSkip = ColBinXSkipMap_One.find(config.binFacCol)->second;
            xSkip = std::max(uint32_t(0), xSkip - config.winStartCol);
            runCommand("set xy skip for one amp", arc::TIM_ID, SXY, xSkip, 0);
        }

        runCommand("set image width", arc::TIM_ID, arc::WRM, (arc::Y_MEM | 1), config.getBinnedWidth());

        runCommand("set image height", arc::TIM_ID, arc::WRM, (arc::Y_MEM | 2), config.getBinnedHeight());

        _config = config;
    }

    void Camera::saveImage() {
        std::cout << "saveImage()\n";
        if (getExposureState().state != StateEnum::ImageRead) {
            throw std::runtime_error("no image available to be read");
        }

        try {
	    arc::gen3::dlace::e_Alg deinterlaceAlgorithm = ReadoutAmpsDeinterlaceAlgorithmMap.find(_config.readoutAmps)->second; 
            //Gotta create a *uint16 from the *uint8 returned by commonBufferVA().
	    uint8_t *commonBufferVA_8=_device.commonBufferVA();
	    uint16_t commonBufferVA_16= 0x0000 | *commonBufferVA_8;
            arc::gen3::CArcDeinterlace deinterlacer;
            std::cout << "deinterlacer.run(" << commonBufferVA_16 
		    << ", " <<  _config.getBinnedHeight()
		    << ", " << _config.getBinnedWidth() 
		    << ", " << uint32_t(deinterlaceAlgorithm) 
		    << ", " << "{}"
		    << ")" << std::endl;
            deinterlacer.run(&commonBufferVA_16,
			    _config.getBinnedHeight(),
			    _config.getBinnedWidth(),
			    deinterlaceAlgorithm, 
			    {}
			    );


            arc::gen3::CArcFitsFile cFits;
            cFits.create(_expName.c_str(), _config.getBinnedHeight(), _config.getBinnedWidth());

            cFits.write(&commonBufferVA_16);

            std::cout << "saved image as \"" << _expName << "\"\n";
        } catch(...) {
            _setIdle();
            throw;
        }
        _setIdle();
    }

    void Camera::openShutter() {
        _assertIdle();
        runCommand("open shutter", arc::TIM_ID, arc::OSH);
    }

    void Camera::closeShutter() {
        _assertIdle();
        runCommand("close shutter", arc::TIM_ID, arc::CSH);
    }

// private methods

    void Camera::_assertIdle() {
        if (this->isBusy()) {
            throw std::runtime_error("busy");
        }
    }

    void Camera::_clearBuffer() {
        std::cout << "_device.fillCommonBuffer(0)\n";
        _device.fillCommonBuffer(0);
        _bufferCleared = true;
    }

    double Camera::_estimateReadTime(int nPix) const {
        return nPix / ReadoutRateFreqMap.find(_config.readoutRate)->second;
    }

    void Camera::_setIdle() {
        _cmdExpSec = -1;
        _estExpSec = -1;
        _segmentExpSec = -1;
        _segmentStartValid = false;
        _clearBuffer();
    }

    void Camera::runCommand(std::string const &descr, uint32_t boardID, uint32_t cmd, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
        if ((boardID != arc::TIM_ID) && (boardID != arc::UTIL_ID) && (boardID != arc::PCI_ID)) {
            std::ostringstream os;
            os << std::hex << "unknown boardID=0x" << boardID;
            throw std::runtime_error(os.str());
        }
        std::cout << std::hex << "_device.command("
            <<  "0x" << boardID
            << ", " << formatCmd(cmd)
            << ", 0x" << arg1
            << ", 0x" << arg2
            << ", 0x" << arg3
            << "): " << descr << std::dec << std::endl;
        int retVal = _device.command({boardID, cmd, arg1, arg2, arg3});
        std::cout << " dev command retVal=" << retVal << std::endl;
        if (retVal != arc::DON) {
            std::ostringstream os;
            os << descr << " failed with retVal=" << retVal;
            throw std::runtime_error(os.str());
        }
    }

} // namespace
