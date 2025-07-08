#include "zip.h"
#include <zlib.h>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iomanip>

// 压缩实现
std::vector<uint8_t> Zip::compress(const std::vector<uint8_t>& data, int level) {
    if (data.empty()) return {};
    
    // 验证压缩级别
    if (level < 0 || level > 9) {
        throw std::invalid_argument("Compression level must be between 0 and 9");
    }
    
    // 计算最大压缩大小
    uLongf compressedSize = compressBound(data.size());
    std::vector<uint8_t> result(compressedSize);
    
    // 执行压缩
    int err = compress2(result.data(), &compressedSize, 
                       data.data(), data.size(), 
                       level);
    
    // 错误处理
    if (err != Z_OK) {
        throw std::runtime_error("Compression failed: " + std::to_string(err));
    }
    
    result.resize(compressedSize);
    return result;
}

// 解压实现
std::vector<uint8_t> Zip::decompress(const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) return {};
    
    // 初始缓冲区大小
    uLongf uncompressedSize = compressed.size() * 10;
    std::vector<uint8_t> result;
    
    do {
        result.resize(uncompressedSize);
        int err = uncompress(result.data(), &uncompressedSize, 
                            compressed.data(), compressed.size());
        
        if (err == Z_OK) {
            result.resize(uncompressedSize);
            return result;
        }
        
        if (err != Z_BUF_ERROR) {
            throw std::runtime_error("Decompression failed: " + std::to_string(err));
        }
        
        // 缓冲区不足时加倍
        uncompressedSize *= 2;
    } while (uncompressedSize < 100 * 1024 * 1024); // 限制100MB
    
    throw std::runtime_error("Decompression failed: output data too large");
}

// 压缩字符串
std::vector<uint8_t> Zip::compressString(const std::string& str, int level) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return compress(data, level);
}

// 解压字符串
std::string Zip::decompressString(const std::vector<uint8_t>& compressed) {
    auto decompressed = decompress(compressed);
    return std::string(decompressed.begin(), decompressed.end());
}

// 压缩文件
void Zip::compressFile(const std::string& inputPath, const std::string& outputPath, int level) {
    // 读取文件内容
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open input file: " + inputPath);
    
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    
    // 压缩数据
    auto compressed = compress(data, level);
    
    // 写入压缩文件
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open output file: " + outputPath);
    
    out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
}

// 解压文件
void Zip::decompressFile(const std::string& inputPath, const std::string& outputPath) {
    // 读取压缩文件
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open input file: " + inputPath);
    
    std::vector<uint8_t> compressed(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    
    // 解压数据
    auto decompressed = decompress(compressed);
    
    // 写入解压文件
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open output file: " + outputPath);
    
    out.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
}

// 流式压缩
void Zip::compressStream(std::istream& input, std::ostream& output, int level) {
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB
    std::vector<uint8_t> inBuf(CHUNK_SIZE);
    std::vector<uint8_t> outBuf(CHUNK_SIZE * 2); // 压缩后可能更大
    
    z_stream stream = {};
    deflateInit2(&stream, level, Z_DEFLATED, MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    
    int flush;
    do {
        input.read(reinterpret_cast<char*>(inBuf.data()), CHUNK_SIZE);
        stream.avail_in = static_cast<uInt>(input.gcount());
        flush = input.eof() ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = inBuf.data();
        
        do {
            stream.avail_out = static_cast<uInt>(outBuf.size());
            stream.next_out = outBuf.data();
            deflate(&stream, flush);
            
            size_t have = outBuf.size() - stream.avail_out;
            output.write(reinterpret_cast<const char*>(outBuf.data()), have);
        } while (stream.avail_out == 0);
    } while (flush != Z_FINISH);
    
    deflateEnd(&stream);
}

// 流式解压
void Zip::decompressStream(std::istream& input, std::ostream& output) {
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB
    std::vector<uint8_t> inBuf(CHUNK_SIZE);
    std::vector<uint8_t> outBuf(CHUNK_SIZE * 2);
    
    z_stream stream = {};
    inflateInit2(&stream, MAX_WBITS);
    
    int ret;
    do {
        input.read(reinterpret_cast<char*>(inBuf.data()), CHUNK_SIZE);
        stream.avail_in = static_cast<uInt>(input.gcount());
        if (stream.avail_in == 0) break;
        stream.next_in = inBuf.data();
        
        do {
            stream.avail_out = static_cast<uInt>(outBuf.size());
            stream.next_out = outBuf.data();
            ret = inflate(&stream, Z_NO_FLUSH);
            
            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&stream);
                throw std::runtime_error("Decompression error: " + std::to_string(ret));
            }
            
            size_t have = outBuf.size() - stream.avail_out;
            output.write(reinterpret_cast<const char*>(outBuf.data()), have);
        } while (stream.avail_out == 0);
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&stream);
}

// 检查是否为ZIP格式
bool Zip::isZipFormat(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    // 简单的魔法数字检查
    return (data[0] == 0x78 && (data[1] == 0x01 || data[1] == 0x5E || 
                                data[1] == 0x9C || data[1] == 0xDA));
}

// 计算压缩率
double Zip::compressionRatio(size_t originalSize, size_t compressedSize) {
    if (originalSize == 0) return 0.0;
    return static_cast<double>(compressedSize) / originalSize * 100.0;
}