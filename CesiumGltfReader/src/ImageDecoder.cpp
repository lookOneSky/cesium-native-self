#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Ktx2TranscodeTargets.h>
#include <CesiumGltfReader/ImageDecoder.h>
#include <CesiumUtility/Assert.h>
#include <CesiumUtility/Tracing.h>

#include <ktx.h>
#include <turbojpeg.h>
#include <webp/decode.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

#pragma region Das
#include <sstream>
#include <openssl/md5.h>
#include <iomanip>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstdlib> // For system()
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stb_image_write.h>

#include <ktx.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

//KTX纹理设置
//#define USE_OUTSIDE_KTX_TEX
//#define CREATE_OUTSIDE_KTX_TEX

namespace fs = std::filesystem;

#define Cache_Path "./Cache/"

//std::string calculateMD5(const std::span<const std::byte>& data) {
//  // MD5 digest is 16 bytes
//  unsigned char digest[MD5_DIGEST_LENGTH];
//
//  // Compute the MD5 hash
//  MD5(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
//
//  // Convert the digest to a hexadecimal string
//  std::ostringstream md5String;
//  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
//    md5String << std::hex << std::setw(2) << std::setfill('0')
//              << static_cast<int>(digest[i]);
//  }
//
//  return md5String.str();
//}

#pragma endregion

#define STBI_FAILURE_USERMSG

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ASSERT(x) CESIUM_ASSERT(x)
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include <stb_image_resize2.h>

namespace CesiumGltfReader {

using namespace CesiumGltf;

namespace {

bool isKtx(const std::span<const std::byte>& data) {
  const size_t ktxMagicByteLength = 12;
  if (data.size() < ktxMagicByteLength) {
    return false;
  }

  const uint8_t ktxMagic[ktxMagicByteLength] =
      {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

  return memcmp(
             data.data(),
             reinterpret_cast<const void*>(ktxMagic),
             ktxMagicByteLength) == 0;
}

bool isWebP(const std::span<const std::byte>& data) {
  if (data.size() < 12) {
    return false;
  }
  const uint32_t magic1 = *reinterpret_cast<const uint32_t*>(data.data());
  const uint32_t magic2 = *reinterpret_cast<const uint32_t*>(data.data() + 8);
  return magic1 == 0x46464952 && magic2 == 0x50424557;
}

} // namespace

#pragma region Das
void ensureDirectoryExists(const std::string& filePath)
{
  // 提取文件夹路径
  fs::path folderPath = fs::path(filePath).parent_path();

  // 检查并创建文件夹
  if (!folderPath.empty() && !fs::exists(folderPath)) {
    if (!fs::create_directories(folderPath)) {
      throw std::runtime_error(
          "Failed to create directory: " + folderPath.string());
    }
  }
}

ImageReaderResult ImageDecoder::readImage(const std::span<const std::byte>& data, const Ktx2TranscodeTargets& ktx2TranscodeTargets)
{
	CESIUM_TRACE("CesiumGltfReader::readImage");

#ifdef USE_OUTSIDE_KTX_TEX
	bool isKTX = isKtx(data);
	if (!isKTX)
  {
		ImageReaderResult result;
		// 不是ktx纹理，纹理加载最耗时
		std::string md5Hash = calculateMD5(data);
		std::string ktx2FilePath = Cache_Path + md5Hash + ".ktx2";
		if (fs::exists(ktx2FilePath)) {
			// 如果文件存在，直接读取 KTX2 文件
			std::ifstream ktx2File(ktx2FilePath, std::ios::binary | std::ios::ate);
			if (!ktx2File) {
				result.errors.emplace_back("Failed to open KTX2 file: " + ktx2FilePath);
				return readImageSrc(data, ktx2TranscodeTargets);
			}

			std::streamsize size = ktx2File.tellg();
			ktx2File.seekg(0, std::ios::beg);

			std::vector<std::byte> ktx2Data(static_cast<size_t>(size));
			if (!ktx2File.read(reinterpret_cast<char*>(ktx2Data.data()), size)) {
				result.errors.emplace_back("Failed to read KTX2 file: " + ktx2FilePath);
				return result;
			}

			// 使用 KTX2 数据调用现有逻辑
			return readImageSrc(ktx2Data, ktx2TranscodeTargets);
		}
		else
    {
#ifdef CREATE_OUTSIDE_KTX_TEX
			//return readImageSrc(data, ktx2TranscodeTargets);
			CesiumGltf::ImageAsset& image = result.pImage.emplace();
			// 4. 如果 KTX2 文件不存在，保存 WebP 数据到临时文件
			std::string tempWebPFilePath = Cache_Path + md5Hash + ".png";
			ensureDirectoryExists(tempWebPFilePath);
			if (WebPGetInfo(
				reinterpret_cast<const uint8_t*>(data.data()),
				data.size(),
				&image.width,
				&image.height)) {
				image.channels = 4;
				image.bytesPerChannel = 1;
				uint8_t* pImage = nullptr;
				const auto bufferSize = image.width * image.height * image.channels;
				image.pixelData.resize(static_cast<std::size_t>(bufferSize));
				pImage = WebPDecodeRGBAInto(
					reinterpret_cast<const uint8_t*>(data.data()),
					data.size(),
					reinterpret_cast<uint8_t*>(image.pixelData.data()),
					image.pixelData.size(),
					image.width * image.channels);

				// 假设 `image.pixelData` 包含解码后的 RGBA 数据
				if (!stbi_write_png(
					tempWebPFilePath.c_str(),
					image.width,
					image.height,
					image.channels,
					image.pixelData.data(),
					image.width * image.channels)) {
					result.errors.emplace_back(
						"Failed to create temporary PNG file: " + tempWebPFilePath);
					return readImageSrc(data, ktx2TranscodeTargets);
				}

				// 5. 使用 toktx 转换 WebP 为 KTX2
        std::string command = "toktx --bcmp --t2 --target_type RGBA " +
          ktx2FilePath + " " + tempWebPFilePath;
				int retCode = std::system(command.c_str());
				if (retCode != 0) {
					result.errors.emplace_back(
						"Failed to convert WebP to KTX2 using toktx. Command: " +
						command);
					fs::remove(ktx2FilePath);
					return readImageSrc(data, ktx2TranscodeTargets);
				}

				// 6. 删除临时 WebP 文件
				fs::remove(tempWebPFilePath);

				// 7. 读取生成的 KTX2 文件
				std::ifstream ktx2File(ktx2FilePath, std::ios::binary | std::ios::ate);
				if (!ktx2File)
        {
					result.errors.emplace_back(
						"Failed to open generated KTX2 file: " + ktx2FilePath);
					return readImageSrc(data, ktx2TranscodeTargets);
				}

				std::streamsize size = ktx2File.tellg();
				ktx2File.seekg(0, std::ios::beg);

				std::vector<std::byte> ktx2Data(static_cast<size_t>(size));
				if (!ktx2File.read(reinterpret_cast<char*>(ktx2Data.data()), size))
        {
					result.errors.emplace_back(
						"Failed to read generated KTX2 file: " + ktx2FilePath);
					return readImageSrc(data, ktx2TranscodeTargets);
				}

				// 使用 KTX2 数据调用现有逻辑
				return readImageSrc(ktx2Data, ktx2TranscodeTargets);
			}
 #endif
		}
	}
#endif
	return readImageSrc(data, ktx2TranscodeTargets);
}
#pragma endregion

/*static*/
ImageReaderResult ImageDecoder::readImageSrc(
    const std::span<const std::byte>& data,
    const Ktx2TranscodeTargets& ktx2TranscodeTargets) {
  CESIUM_TRACE("CesiumGltfReader::readImage");

  ImageReaderResult result;

  CesiumGltf::ImageAsset& image = result.pImage.emplace();

  if (isKtx(data)) {
    ktxTexture2* pTexture = nullptr;
    KTX_error_code errorCode;

    errorCode = ktxTexture2_CreateFromMemory(
        reinterpret_cast<const std::uint8_t*>(data.data()),
        data.size(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &pTexture);

    if (errorCode == KTX_SUCCESS) {
      if (ktxTexture2_NeedsTranscoding(pTexture)) {

        CESIUM_TRACE("Transcode KTXv2");

        image.channels =
            static_cast<int32_t>(ktxTexture2_GetNumComponents(pTexture));
        GpuCompressedPixelFormat transcodeTargetFormat =
            GpuCompressedPixelFormat::NONE;

        if (pTexture->supercompressionScheme == KTX_SS_BASIS_LZ) {
          switch (image.channels) {
          case 1:
            transcodeTargetFormat = ktx2TranscodeTargets.ETC1S_R;
            break;
          case 2:
            transcodeTargetFormat = ktx2TranscodeTargets.ETC1S_RG;
            break;
          case 3:
            transcodeTargetFormat = ktx2TranscodeTargets.ETC1S_RGB;
            break;
          // case 4:
          default:
            transcodeTargetFormat = ktx2TranscodeTargets.ETC1S_RGBA;
          }
        } else {
          switch (image.channels) {
          case 1:
            transcodeTargetFormat = ktx2TranscodeTargets.UASTC_R;
            break;
          case 2:
            transcodeTargetFormat = ktx2TranscodeTargets.UASTC_RG;
            break;
          case 3:
            transcodeTargetFormat = ktx2TranscodeTargets.UASTC_RGB;
            break;
          // case 4:
          default:
            transcodeTargetFormat = ktx2TranscodeTargets.UASTC_RGBA;
          }
        }

        ktx_transcode_fmt_e transcodeTargetFormat_ = KTX_TTF_RGBA32;
        switch (transcodeTargetFormat) {
        case GpuCompressedPixelFormat::ETC1_RGB:
          transcodeTargetFormat_ = KTX_TTF_ETC1_RGB;
          break;
        case GpuCompressedPixelFormat::ETC2_RGBA:
          transcodeTargetFormat_ = KTX_TTF_ETC2_RGBA;
          break;
        case GpuCompressedPixelFormat::BC1_RGB:
          transcodeTargetFormat_ = KTX_TTF_BC1_RGB;
          break;
        case GpuCompressedPixelFormat::BC3_RGBA:
          transcodeTargetFormat_ = KTX_TTF_BC3_RGBA;
          break;
        case GpuCompressedPixelFormat::BC4_R:
          transcodeTargetFormat_ = KTX_TTF_BC4_R;
          break;
        case GpuCompressedPixelFormat::BC5_RG:
          transcodeTargetFormat_ = KTX_TTF_BC5_RG;
          break;
        case GpuCompressedPixelFormat::BC7_RGBA:
          transcodeTargetFormat_ = KTX_TTF_BC7_RGBA;
          break;
        case GpuCompressedPixelFormat::PVRTC1_4_RGB:
          transcodeTargetFormat_ = KTX_TTF_PVRTC1_4_RGB;
          break;
        case GpuCompressedPixelFormat::PVRTC1_4_RGBA:
          transcodeTargetFormat_ = KTX_TTF_PVRTC1_4_RGBA;
          break;
        case GpuCompressedPixelFormat::ASTC_4x4_RGBA:
          transcodeTargetFormat_ = KTX_TTF_ASTC_4x4_RGBA;
          break;
        case GpuCompressedPixelFormat::PVRTC2_4_RGB:
          transcodeTargetFormat_ = KTX_TTF_PVRTC2_4_RGB;
          break;
        case GpuCompressedPixelFormat::PVRTC2_4_RGBA:
          transcodeTargetFormat_ = KTX_TTF_PVRTC2_4_RGBA;
          break;
        case GpuCompressedPixelFormat::ETC2_EAC_R11:
          transcodeTargetFormat_ = KTX_TTF_ETC2_EAC_R11;
          break;
        case GpuCompressedPixelFormat::ETC2_EAC_RG11:
          transcodeTargetFormat_ = KTX_TTF_ETC2_EAC_RG11;
          break;
        // case NONE:
        default:
          transcodeTargetFormat_ = KTX_TTF_RGBA32;
          break;
        };

        errorCode =
            ktxTexture2_TranscodeBasis(pTexture, transcodeTargetFormat_, 0);
        if (errorCode != KTX_SUCCESS) {
          transcodeTargetFormat_ = KTX_TTF_RGBA32;
          transcodeTargetFormat = GpuCompressedPixelFormat::NONE;
          errorCode =
              ktxTexture2_TranscodeBasis(pTexture, transcodeTargetFormat_, 0);
        }
        if (errorCode == KTX_SUCCESS) {
          image.compressedPixelFormat = transcodeTargetFormat;
          image.width = static_cast<int32_t>(pTexture->baseWidth);
          image.height = static_cast<int32_t>(pTexture->baseHeight);

          if (transcodeTargetFormat == GpuCompressedPixelFormat::NONE) {
            // We fully decompressed the texture in this case.
            image.bytesPerChannel = 1;
            image.channels = 4;
          }

          // In the KTX2 spec, there's a distinction between "this image has no
          // mipmaps, so they should be generated at runtime" and "this
          // image has no mipmaps because it makes no sense to create a mipmap
          // for this type of image." It is, confusingly, encoded in the
          // `levelCount` property:
          // https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_levelcount
          //
          // With `levelCount=0`, mipmaps should be generated. With
          // `levelCount=1`, mipmaps make no sense. So when `levelCount=0`, we
          // want to leave the `mipPositions` array _empty_. With
          // `levelCount=1`, we want to populate it with a single mip level.
          //
          // However, this `levelCount` property is not directly exposed by the
          // KTX2 loader API we're using here. Instead, there is a `numLevels`
          // property, but it will _never_ have the value 0, because it
          // represents the number of levels of actual pixel data we have. When
          // the API sees `levelCount=0`, it will assign the value 1 to
          // `numLevels`, but it will _also_ set `generateMipmaps` to true.
          //
          // The API docs say that `numLevels` will always be 1 when
          // `generateMipmaps` is true.
          //
          // So, in summary, when `generateMipmaps=false`, we populate
          // `mipPositions` with whatever mip levels the KTX provides and we
          // don't generate any more. When it's true, we treat all the image
          // data as belonging to a single base-level image and generate mipmaps
          // from that if necessary.
          if (!pTexture->generateMipmaps) {
            // Copy over the positions of each mip within the buffer.
            image.mipPositions.resize(pTexture->numLevels);
            for (ktx_uint32_t level = 0; level < pTexture->numLevels; ++level) {
              ktx_size_t imageOffset;
              ktxTexture_GetImageOffset(
                  ktxTexture(pTexture),
                  level,
                  0,
                  0,
                  &imageOffset);
              ktx_size_t imageSize =
                  ktxTexture_GetImageSize(ktxTexture(pTexture), level);

              image.mipPositions[level] = {imageOffset, imageSize};
            }
          } else {
            CESIUM_ASSERT(pTexture->numLevels == 1);
          }

          // Copy over the entire buffer, including all mips.
          ktx_uint8_t* pixelData = ktxTexture_GetData(ktxTexture(pTexture));
          ktx_size_t pixelDataSize =
              ktxTexture_GetDataSize(ktxTexture(pTexture));

          image.pixelData.resize(pixelDataSize);
          std::uint8_t* u8Pointer =
              reinterpret_cast<std::uint8_t*>(image.pixelData.data());
          std::copy(pixelData, pixelData + pixelDataSize, u8Pointer);

          ktxTexture_Destroy(ktxTexture(pTexture));

          return result;
        }
      }
    }

    result.pImage = nullptr;
    result.errors.emplace_back(
        "KTX2 loading failed with error: " +
        std::string(ktxErrorString(errorCode)));

    return result;
  } else if (isWebP(data)) {
    if (WebPGetInfo(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size(),
            &image.width,
            &image.height)) {
      image.channels = 4;
      image.bytesPerChannel = 1;
      uint8_t* pImage = nullptr;
      const auto bufferSize = image.width * image.height * image.channels;
      image.pixelData.resize(static_cast<std::size_t>(bufferSize));
      pImage = WebPDecodeRGBAInto(
          reinterpret_cast<const uint8_t*>(data.data()),
          data.size(),
          reinterpret_cast<uint8_t*>(image.pixelData.data()),
          image.pixelData.size(),
          image.width * image.channels);
      if (!pImage) {
        result.pImage = nullptr;
        result.errors.emplace_back("Unable to decode WebP");
      }
      return result;
    }
  }

  {
    tjhandle tjInstance = tjInitDecompress();
    int inSubsamp, inColorspace;
    if (!tjDecompressHeader3(
            tjInstance,
            reinterpret_cast<const unsigned char*>(data.data()),
            static_cast<unsigned long>(data.size()), // NOLINT
            &image.width,
            &image.height,
            &inSubsamp,
            &inColorspace)) {
      CESIUM_TRACE("Decode JPG");
      image.bytesPerChannel = 1;
      image.channels = 4;
      const auto lastByte =
          image.width * image.height * image.channels * image.bytesPerChannel;
      image.pixelData.resize(static_cast<std::size_t>(lastByte));
      if (tjDecompress2(
              tjInstance,
              reinterpret_cast<const unsigned char*>(data.data()),
              static_cast<unsigned long>(data.size()), // NOLINT
              reinterpret_cast<unsigned char*>(image.pixelData.data()),
              image.width,
              0,
              image.height,
              TJPF_RGBA,
              0)) {
        result.errors.emplace_back("Unable to decode JPEG");
        result.pImage = nullptr;
      }
    } else {
      CESIUM_TRACE("Decode PNG");
      image.bytesPerChannel = 1;
      image.channels = 4;

      int channelsInFile;
      stbi_uc* pImage = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(data.data()),
          static_cast<int>(data.size()),
          &image.width,
          &image.height,
          &channelsInFile,
          image.channels);
      if (pImage) {
        CESIUM_TRACE(
            "copy image " + std::to_string(image.width) + "x" +
            std::to_string(image.height) + "x" +
            std::to_string(image.channels) + "x" +
            std::to_string(image.bytesPerChannel));
        // std::uint8_t is not implicitly convertible to std::byte, so we must
        // use reinterpret_cast to (safely) force the conversion.
        const auto lastByte =
            image.width * image.height * image.channels * image.bytesPerChannel;
        image.pixelData.resize(static_cast<std::size_t>(lastByte));
        std::uint8_t* u8Pointer =
            reinterpret_cast<std::uint8_t*>(image.pixelData.data());
        std::copy(pImage, pImage + lastByte, u8Pointer);
        stbi_image_free(pImage);
      } else {
        result.pImage = nullptr;
        result.errors.emplace_back(stbi_failure_reason());
      }
    }
    tjDestroy(tjInstance);
  }
  return result;
}

/*static*/
std::optional<std::string> ImageDecoder::generateMipMaps(ImageAsset& image) {
  if (!image.mipPositions.empty() ||
      image.compressedPixelFormat != GpuCompressedPixelFormat::NONE) {
    // No error message needed, since this is not technically a failure.
    return std::nullopt;
  }

  if (image.pixelData.empty()) {
    return "Unable to generate mipmaps, an empty image was provided.";
  }

  CESIUM_TRACE(
      "generate mipmaps " + std::to_string(image.width) + "x" +
      std::to_string(image.height) + "x" + std::to_string(image.channels) +
      "x" + std::to_string(image.bytesPerChannel));

  int32_t mipWidth = image.width;
  int32_t mipHeight = image.height;
  int32_t totalPixelCount = mipWidth * mipHeight;
  size_t mipCount = 1;
  while (mipWidth > 1 || mipHeight > 1) {
    ++mipCount;

    if (mipWidth > 1) {
      mipWidth >>= 1;
    }

    if (mipHeight > 1) {
      mipHeight >>= 1;
    }

    // Total pixels in the final mipmap.
    totalPixelCount += mipWidth * mipHeight;
  }

  // Byte size of the base image.
  const size_t imageByteSize = static_cast<size_t>(
      image.width * image.height * image.channels * image.bytesPerChannel);

  image.mipPositions.resize(mipCount);
  image.mipPositions[0].byteOffset = 0;
  image.mipPositions[0].byteSize = imageByteSize;

  image.pixelData.resize(static_cast<size_t>(
      totalPixelCount * image.channels * image.bytesPerChannel));

  mipWidth = image.width;
  mipHeight = image.height;
  size_t mipIndex = 0;
  size_t byteOffset = 0;
  size_t byteSize = imageByteSize;
  while (mipWidth > 1 || mipHeight > 1) {
    size_t lastByteOffset = byteOffset;
    byteOffset += byteSize;
    ++mipIndex;

    int32_t lastWidth = mipWidth;
    if (mipWidth > 1) {
      mipWidth >>= 1;
    }

    int32_t lastHeight = mipHeight;
    if (mipHeight > 1) {
      mipHeight >>= 1;
    }

    byteSize = static_cast<size_t>(
        mipWidth * mipHeight * image.channels * image.bytesPerChannel);

    image.mipPositions[mipIndex].byteOffset = byteOffset;
    image.mipPositions[mipIndex].byteSize = byteSize;

    if (!ImageDecoder::unsafeResize(
            &image.pixelData[lastByteOffset],
            lastWidth,
            lastHeight,
            0,
            &image.pixelData[byteOffset],
            mipWidth,
            mipHeight,
            0,
            image.channels)) {
      // Remove any added mipmaps.
      image.mipPositions.clear();
      image.pixelData.resize(imageByteSize);
      return stbi_failure_reason();
    }
  }

  return std::nullopt;
}

/*static*/ bool ImageDecoder::unsafeResize(
    const std::byte* pInputPixels,
    int32_t inputWidth,
    int32_t inputHeight,
    int32_t inputStrideBytes,
    std::byte* pOutputPixels,
    int32_t outputWidth,
    int32_t outputHeight,
    int32_t outputStrideBytes,
    int32_t channels) {
  return stbir_resize_uint8_linear(
             reinterpret_cast<const unsigned char*>(pInputPixels),
             inputWidth,
             inputHeight,
             inputStrideBytes,
             reinterpret_cast<unsigned char*>(pOutputPixels),
             outputWidth,
             outputHeight,
             outputStrideBytes,
             static_cast<stbir_pixel_layout>(channels)) != nullptr;
}

} // namespace CesiumGltfReader
