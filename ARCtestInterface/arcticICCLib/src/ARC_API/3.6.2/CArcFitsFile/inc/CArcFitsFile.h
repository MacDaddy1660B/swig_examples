/*! \file CArcFitsFile.h */
// +------------------------------------------------------------------------------------------------------------------+
// |  FILE:  CArcFitsFile.h  ( Gen3 )                                                                                 |
// +------------------------------------------------------------------------------------------------------------------+
// |  PURPOSE: This file defines the standard ARC FITS interface.                                                     |
// |                                                                                                                  |
// |  AUTHOR:  Scott Streit			DATE: March 25, 2020                                                             |
// |                                                                                                                  |
// |  Copyright 2013 Astronomical Research Cameras, Inc. All rights reserved.                                         |
// +------------------------------------------------------------------------------------------------------------------+

#pragma once

#ifdef _WINDOWS
#pragma warning( disable: 4251 )
#endif

#include <filesystem>
#include <version>
#include <stdexcept>
#include <cstdint>
#include <utility>
#include <memory>
#include <string>

#ifdef __cpp_lib_variant
	#include <variant>
#else
	#include <tuple>
#endif

#include <CArcFitsFileDllMain.h>
#include <CArcStringList.h>
#include <CArcBase.h>

#include <fitsio.h>		// This header MUST be last to prevent winnt.h constant error!


namespace arc
{
	namespace gen3
	{

		namespace fits
		{

			/** 16 bits-per-pixel image data. */
			using BPP_16 = std::uint16_t;

			/** 32 bits-per-pixel image data. */
			using BPP_32 = std::uint32_t;


			/** Keyword return type */
			#ifdef __cpp_lib_variant
				using keywordValue_t = std::variant<std::uint32_t, std::int32_t, std::uint64_t, std::int64_t, double, std::string>;
			#else
				using keywordValue_t = std::tuple<std::uint32_t, std::int32_t, std::uint64_t, std::int64_t, double, std::string>;
			#endif


			/** @enum arc::gen3::fits::e_ReadMode
			 *  Defines the allowable modes of operation for opening an existing file.
			 *  @var arc::gen3::fits::e_ReadMode::READMODE
			 *  Create or open the FITS file in read only mode.
			 *  @var arc::gen3::fits::e_ReadMode::READWRITEMODE
			 *  Create or open the FITS file in read-write mode.
			 */
			enum class e_ReadMode : std::int32_t
			{
				READMODE = READONLY,
				READWRITEMODE = READWRITE
			};


			/** @enum arc::gen3::fits::e_Type
			 *  Defines the allowable types for header keywords.
			 *  @var arc::gen3::fits::e_Type::FITS_INVALID_KEY	- Invalid type
			 *  @var arc::gen3::fits::e_Type::FITS_STRING_KEY	- String type
			 *  @var arc::gen3::fits::e_Type::FITS_INT_KEY		- Integer type
			 *  @var arc::gen3::fits::e_Type::FITS_UINT_KEY		- Unsigned integer type
			 *  @var arc::gen3::fits::e_Type::FITS_SHORT_KEY	- Short type
			 *  @var arc::gen3::fits::e_Type::FITS_USHORT_KEY	- Unsigned short type
			 *  @var arc::gen3::fits::e_Type::FITS_FLOAT_KEY	- Float type
			 *  @var arc::gen3::fits::e_Type::FITS_DOUBLE_KEY	- Double type
			 *  @var arc::gen3::fits::e_Type::FITS_BYTE_KEY		- Byte type
			 *  @var arc::gen3::fits::e_Type::FITS_LONG_KEY		- Long type
			 *  @var arc::gen3::fits::e_Type::FITS_ULONG_KEY	- Unsigned long type
			 *  @var arc::gen3::fits::e_Type::FITS_LONGLONG_KEY	- Long long type
			 *  @var arc::gen3::fits::e_Type::FITS_LOGICAL_KEY	- Boolean type
			 *  @var arc::gen3::fits::e_Type::FITS_COMMENT_KEY	- String type
			 *  @var arc::gen3::fits::e_Type::FITS_HISTORY_KEY	- String type
			 *  @var arc::gen3::fits::e_Type::FITS_DATE_KEY		- String type
			 */
			enum class e_Type : std::int32_t
			{
				FITS_INVALID_KEY = -1,
				FITS_STRING_KEY = 0,
				FITS_INT_KEY,
				FITS_UINT_KEY,
				FITS_SHORT_KEY,
				FITS_USHORT_KEY,
				FITS_FLOAT_KEY,
				FITS_DOUBLE_KEY,
				FITS_BYTE_KEY,
				FITS_LONG_KEY,
				FITS_ULONG_KEY,
				FITS_LONGLONG_KEY,
				FITS_LOGICAL_KEY,
				FITS_COMMENT_KEY,
				FITS_HISTORY_KEY,
				FITS_DATE_KEY
			};


			/** Image parameters info class. */
			class GEN3_CARCFITSFILE_API CParam
			{
				public:

					/** Returns the image column pixel length.
					 *	@returns The number of image columns.
					 */
					std::uint32_t getCols( void ) noexcept;

					/** Returns the image row pixel length.
					 *	@returns The number of image rows.
					 */
					std::uint32_t getRows( void ) noexcept;

					/** Returns the number of frames in the file.
					 *	@returns The image frame count ( 1 = single, non-data cube, image ).
					 */
					std::uint32_t getFrames( void ) noexcept;

					/** Returns the number of axes in the file.
					 *	@returns The image axis count ( 2 = normal image, 3 = data cube ).
					 */
					std::uint32_t getNAxis( void ) noexcept;

					/** Returns the image bits-per-pixel value.
					 *  @returns The number of bits - per - pixel in the image.
					 */
					std::uint32_t getBpp( void ) noexcept;

					/** Constructor */
					CParam( void );

					/** Destructor */
					~CParam( void );

					//  Data members. Although public, intended for interal use only.
					// +----------------------------------------------------------------+

					/** The image dimensions and frame count. Index 0: columns, 1: rows, 2: frame count */
					long m_lNAxes[ 3 ];

					/** The image columns (in pixels) */
					long* m_plCols;

					/** The image rows (in pixels) */
					long* m_plRows;

					/** The number of image frames */
					long* m_plFrames;

					/** The number of axes in the image. A standard image has two; a data cube three. */
					int m_iNAxis;

					/** The number of bits-per-pixel in the image */
					int m_iBpp;

					// +----------------------------------------------------------------+
			};


			/** @struct ArrayDeleter
			 *  Returned array deleter
			 */
			template <typename T>
			struct GEN3_CARCFITSFILE_API ArrayDeleter
			{
				void operator()( T* p ) const
				{
					if ( p != nullptr )
					{
						delete[] p;
					}
				}
			};


			// +----------------------------------------------------------------------------------------------------------+
			// |  Definitions for Point data type                                                                         |
			// +----------------------------------------------------------------------------------------------------------+

			/** Point parameter type definition
			 */
			using Point = std::pair<long, long>;


			/** Macro to create a point parameter.
			 *  @param uiColumn - The point column.
			 *  @param uiRow    - The point row.
			 */
			constexpr auto MAKE_POINT( std::uint32_t uiColumn, std::uint32_t uiRow ) { return std::make_pair( uiColumn, uiRow ); }

		}	// end fits namespace


		/** @class CArcFitsFile
		 *  ARC FITS file interface class. Utilizes the cfitsio library for all actions.
		 *  @see arc::gen3::CArcBase
		 */
		template <typename T = arc::gen3::fits::BPP_16>
		class GEN3_CARCFITSFILE_API CArcFitsFile : public arc::gen3::CArcBase
		{
			public:

				/** Constructor
				 *  Creates an empty FITS file object.
				 */
				CArcFitsFile( void );

				/** Destructor
				 */
				virtual ~CArcFitsFile( void );

				/** Returns a textual representation of the library version.
				 *  @return A string representation of the library version.
				 */
				static const std::string version( void ) noexcept;

				/** Returns a textual representation of the cfitsio library version.
				 *  @return A string representation of the cfitsio library version.
				 */
				static const std::string cfitsioVersion( void ) noexcept;

				/** Returns the class bits-per-pixel typename.
				 *  @return The class bits-per-pixel typename.
				 */
				std::string getType( void ) noexcept;

				/** Creates a new single image file on disk with the specified image dimensions.
				 *  @param tFileName - The new FITS file name.
				 *  @param uiCols	 - The image column size ( in pixels ).
				 *  @param uiRows	 - The image row size ( in pixels ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void create( const std::filesystem::path& tFileName, const std::uint32_t uiCols, const std::uint32_t uiRows );

				/** Creates a new data cube file on disk with the specified image dimensions.
				 *  @param tFileName - The new FITS file name.
				 *  @param uiCols	 - The image column size ( in pixels ).
				 *  @param uiRows	 - The image row size ( in pixels ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void create3D( const std::filesystem::path& tFileName, const std::uint32_t uiCols, const std::uint32_t uiRows );

				/** Opens an existing file. Can be used to open a file containing a single image or data cube
				 *  ( a file with multiple image planes ).
				 *  @param tFileName - The file name.
				 *  @param eMode - The mode with which to open the file ( default = e_ReadMode::READMODE ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void open( const std::filesystem::path& tFileName, arc::gen3::fits::e_ReadMode eMode = arc::gen3::fits::e_ReadMode::READMODE );

				/** Closes the file.
				 */
				void close( void ) noexcept;

				/** Returns the FITS header as a list of strings.
				 *  @see arc::gen3::CArcStringList
				 *  @see std::unique_ptr
				 *  @return A std::unique_ptr to a arc::gen3::CArcStringList.
				 *  @throws std::runtime_error on error.
				 */
				std::unique_ptr<arc::gen3::CArcStringList> getHeader( void );

				/** Returns the file name.
				 *  @return The FITS filename associated with this object.
				 *  @throws std::runtime_error on error.
				 */
				const std::string getFileName( void );

				/** Reads a keyword value from the header.
				 *  @param sKey		- A valid FITS keyword.
				 *  @param eType	- The keyword value type.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				arc::gen3::fits::keywordValue_t readKeyword( const std::string& sKey, arc::gen3::fits::e_Type eType );

				/** Writes a new keyword to the header.
				 *
				 * 'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the keyword
				 *                          is greater than 8 characters, which is the standard FITS keyword length. See the
				 *                          link below for details:
				 *
				 *  http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html
				 *
				 *  @param sKey		- A valid FITS keyword.
				 *  @param pValue	- A pointer to the keyword value. The value type is determined by the eType parameter.
				 *					  Cannot equal nullptr.
				 *  @param eType	- The keyword value type.
				 *  @param sComment	- The optional header comment ( default = " " ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void writeKeyword( const std::string& sKey, void* pValue, arc::gen3::fits::e_Type eType, const std::string& sComment = "" );

				/** Updates an existng header keyword.
				 *
				 * 'HIERARCH' keyword NOTE: This text will be prefixed to any keyword by the cfitsio library if the keyword
				 *                          is greater than 8 characters, which is the standard FITS keyword length. See the
				 *                          link below for details:
				 *
				 *  http://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/f_user/node28.html
				 *
				 *  @param sKey		- A valid FITS keyword.
				 *  @param pValue	- A pointer to the keyword value. The value type is determined by the eType parameter. Must not equal nullptr.
				 *  @param eType	- The keyword value type.
				 *  @param sComment	- The optional header comment ( default = " " ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void updateKeyword( const std::string& sKey, void* pValue, arc::gen3::fits::e_Type eType, const std::string& sComment = "" );

				/** Returns the basic image parameters ( number of cols, rows, frames, dimensions and bits-per-pixel ).
				 *  @returns Pointer to an arc::gen3::fits::CParam class that contains all the image parameters. May return nullptr.
				 *  @throws std::runtime_error on error.
				 */
				std::unique_ptr<arc::gen3::fits::CParam> getParameters( void );

				/** Returns the number of frames.  A single image file will return a value of 0.
				 *  @return	The number of frames.
				 *  @throws std::runtime_error on error.
				 */
				std::uint32_t getNumberOfFrames( void );

				/** Returns the number of rows in the image.
				 *  @return	The number of rows in the image.
				 *  @throws std::runtime_error on error.
				 */
				std::uint32_t getRows( void );

				/** Returns the number of columns in the image.
				 *  @return	The number of columns in the image.
				 *  @throws std::runtime_error on error.
				 */
				std::uint32_t getCols( void );

				/** Returns the number of dimensions in the image.
				 *  @return	The number of dimensions in the image.
				 *  @throws std::runtime_error on error.
				 */
				std::uint32_t getNAxis( void );

				/** Returns the image bits-per-pixel value.
				 *  @return	The image bits-per-pixel value.
				 *  @throws std::runtime_error on error.
				 */
				std::uint32_t getBitsPerPixel( void );

				/** Generates a ramp test pattern image within the file. The size of the image is determined by the image
				 *  dimensions supplied during the create() method call. This method is only valid for single image files.
				 *  @throws std::runtime_error on error.
				 */
				void generateTestData( void );

				/** Effectively closes and re-opens the underlying disk file.
				 *  @throws std::runtime_error on error.
				 */
				void reOpen( void );

				/** Causes all internal data buffers to write data to the disk file.
				 *  @throws std::runtime_error on error.
				 */
				void flush( void );

				/** Compares this file image data to another. This method does not check headers, except for size values.
				 *  @param cFitsFile - A reference to another valid FITS file.
				 */
				void compare( CArcFitsFile<T>& cFitsFile );

				/** Resizes a single image file by modifying the NAXES keyword and increasing the image data portion of the file.
				 *  @param uiCols - The number of cols the new FITS file will have.
				 *  @param uiRows - The number of rows the new FITS file will have.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void reSize( const std::uint32_t uiCols, const std::uint32_t uiRows );

				/** Writes image data to a single image file.
				 *  @param pBuf - The image buffer to write. Buffer access violation results in undefined behavior.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void write( T* pBuf );

				/** Writes the specified number of bytes to a single image file. The start position of the data within
				 *  the file image can be specified.
				 *  @param pBuf		- The image buffer to write. Buffer access violation results in undefined behavior.
				 *  @param i64Bytes	- The number of bytes to write.
				 *  @param i64Pixel	- The start pixel within the file image. ( default = 1 ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void write( T* pBuf, const std::int64_t i64Bytes, const std::int64_t i64Pixel = 1 );

				/** Writes a sub-image of the specified buffer to a single image file.
				 *  @param pBuf				- The image buffer to write. Buffer access violation results in undefined behavior.
				 *  @param lowerLeftPoint	- The lower left point { col, row } of the sub-image.
				 *  @param upperRightPoint	- The upper right point { col, row } of the sub-image.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void writeSubImage( T* pBuf, const arc::gen3::fits::Point& lowerLeftPoint, const arc::gen3::fits::Point& upperRightPoint );

				/** Reads a sub-image from a single image file.
				 *  @param lowerLeftPoint	- The lower left point { col, row } of the sub-image.
				 *  @param upperRightPoint	- The upper right point { col, row } of the sub-image.
				 *  @return A pointer to the sub-image data.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> readSubImage( arc::gen3::fits::Point lowerLeftPoint, arc::gen3::fits::Point upperRightPoint );

				/** Read the image from a single image file.
				 *  @return A pointer to the image data.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> read( void );

				/** Read the image from a single image file into the specified user buffer.
				 *  @param pBuf		- The user supplied buffer.
				 *  @param uiCols	- The buffer column length ( in pixels ).
				 *  @param uiRows	- The buffer row length ( in pixels ).
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 *  @throws std::length_error
				 */
				void read( T* pBuf, const std::uint32_t uiCols, const std::uint32_t uiRows );

				/** Writes an image to the end of a data cube file.
				 *  @param pBuf - The image buffer to write. Buffer access violation results in undefined behavior.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void write3D( T* pBuf );

				/** Re-writes an existing image in a data cube file. The image data MUST match in size to the exising images
				 *  within the data cube.
				 *  @param pBuf				- The image buffer. Buffer access violation results in undefined behavior.
				 *  @param uiImageNumber	- The number of the data cube image to replace.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				void reWrite3D( T* pBuf, const std::uint32_t uiImageNumber );

				/** Reads an image from a data cube file.
				 *  @param uiImageNumber - The image number.
				 *  @return A pointer to the image data.
				 *  @throws std::runtime_error
				 *  @throws std::invalid_argument
				 */
				std::unique_ptr<T[], arc::gen3::fits::ArrayDeleter<T>> read3D( const std::uint32_t uiImageNumber );

				/** Returns the underlying cfitsio file pointer.
				 *  @return A pointer to the internal cfitsio file pointer ( may be nullptr ).
				 */
				fitsfile* getBaseFile( void );

				/** Determines the maximum value for a specific data type. Example, for std::uint16_t: 2^16 = 65536.
				 *  @return The maximum value for the data type currently in use.
				 */
				std::uint32_t maxTVal( void );

			private:

				/** Ensures that the internal fits handle is valid.
				 *  @throws std::runtime_error if the fits handle is not valid.
				 */
				constexpr void verifyFileHandle( void );

				/** Throws std::runtime_error with message based on cfitsio error code.
				 *  @param iStatus - cfitsio return status code.
				 */
				void throwFitsError( const std::int32_t iStatus );

				/** version() text holder */
				static const std::string m_sVersion;

				/** Start pixel for multi-access files */
				std::int64_t m_i64Pixel;

				/** Current frame number for multi-access data cube files */
				std::int32_t m_iFrame;

				/** cfitsio file pointer */
				fitsfile* m_pFits;
		};


	}	// end gen3 namespace
}		// end arc namespace



namespace std
{
	/**
	 *  Creates a modified version of the std::default_delete class for use by
	 *  all std::unique_ptr's returned from CArcFitsFile.
	 */
	template<>
	class GEN3_CARCFITSFILE_API default_delete< arc::gen3::fits::CParam >
	{
	public:

		/** Deletes the specified FITS parameter object
		 *  @param pObj - The FITS parameter object to be deleted/destroyed.
		 */
		void operator()( arc::gen3::fits::CParam* pObj );
	};
}
