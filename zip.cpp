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
        throw std::runtime_error("Compression failed: " + std::string(zError(err)));
    }
    
    result.resize(compressedSize);
    return result;
}

// 解压实现
std::vector<uint8_t> Zip::decompress(const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) return {};
    
    // 初始缓冲区大小
    size_t bufferSize = compressed.size() * 2;
    std::vector<uint8_t> result;
    int err = Z_BUF_ERROR;
    
    // 最多尝试5次
    for (int i = 0; i < 5; ++i) {
        result.resize(bufferSize);
        uLongf destLen = static_cast<uLongf>(result.size());
        
        err = uncompress(result.data(), &destLen, 
                        compressed.data(), compressed.size());
        
        if (err == Z_OK) {
            result.resize(destLen);
            return result;
        }
        
        if (err != Z_BUF_ERROR) {
            throw std::runtime_error("Decompression failed: " + std::string(zError(err)));
        }
        
        // 缓冲区不足时增加50%
        bufferSize = bufferSize * 3 / 2;
    }
    
    throw std::runtime_error("Decompression failed: output buffer too small after multiple attempts");
}

// 压缩字符串
std::vector<uint8_t> Zip::compressString(const std::string& str, int level) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return compress(data, level);
}

// 解压字符串
std::string Zip::decompressString(const std::vector<uint8_t>& compressed) {
    auto decompressed = decompress(compressed);
    return std::string(reinterpret_cast<char*>(decompressed.data()), decompressed.size());
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
    std::vector<uint8_t> outBuf(CHUNK_SIZE * 2);
    
    z_stream stream = {};
    if (deflateInit2(&stream, level, Z_DEFLATED, MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("deflateInit failed: " + std::string(zError(stream.avail_in)));
    }
    
    auto cleanup = [&] { deflateEnd(&stream); };
    
    try {
        int flush;
        do {
            input.read(reinterpret_cast<char*>(inBuf.data()), CHUNK_SIZE);
            stream.avail_in = static_cast<uInt>(input.gcount());
            flush = input.eof() ? Z_FINISH : Z_NO_FLUSH;
            stream.next_in = inBuf.data();
            
            do {
                stream.avail_out = static_cast<uInt>(outBuf.size());
                stream.next_out = outBuf.data();
                
                int err = deflate(&stream, flush);
                if (err == Z_STREAM_ERROR) {
                    throw std::runtime_error("Compression error: " + std::string(zError(err)));
                }
                
                size_t have = outBuf.size() - stream.avail_out;
                output.write(reinterpret_cast<const char*>(outBuf.data()), have);
            } while (stream.avail_out == 0);
        } while (flush != Z_FINISH);
    } catch (...) {
        cleanup();
        throw;
    }
    
    cleanup();
}

// 流式解压
void Zip::decompressStream(std::istream& input, std::ostream& output) {
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB
    std::vector<uint8_t> inBuf(CHUNK_SIZE);
    std::vector<uint8_t> outBuf(CHUNK_SIZE * 2);
    
    z_stream stream = {};
    if (inflateInit2(&stream, MAX_WBITS) != Z_OK) {
        throw std::runtime_error("inflateInit failed: " + std::string(zError(stream.avail_in)));
    }
    
    auto cleanup = [&] { inflateEnd(&stream); };
    
    try {
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
                
                if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || 
                    ret == Z_MEM_ERROR || ret == Z_STREAM_ERROR) {
                    throw std::runtime_error("Decompression error: " + std::string(zError(ret)));
                }
                
                size_t have = outBuf.size() - stream.avail_out;
                output.write(reinterpret_cast<const char*>(outBuf.data()), have);
            } while (stream.avail_out == 0);
        } while (ret != Z_STREAM_END);
        
        if (ret != Z_STREAM_END) {
            throw std::runtime_error("Decompression incomplete");
        }
    } catch (...) {
        cleanup();
        throw;
    }
    
    cleanup();
}

// 检查是否为zlib格式
bool Zip::isZlibFormat(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    // zlib魔法数字检查
    return (data[0] == 0x78 && (data[1] == 0x01 || data[1] == 0x5E || 
                                data[1] == 0x9C || data[1] == 0xDA));
}

// 计算压缩率
double Zip::compressionRatio(size_t originalSize, size_t compressedSize) {
    if (originalSize == 0) return 0.0;
    return static_cast<double>(compressedSize) / originalSize * 100.0;
}