#ifndef ZIP_H
#define ZIP_H

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

class Zip {
public:
    // === 核心压缩/解压功能 ===
    
    // 压缩数据 (一行代码)
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int level = 6);
    
    // 解压数据 (一行代码)
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed);
    
    // === 字符串操作 ===
    
    // 压缩字符串 (一行代码)
    static std::vector<uint8_t> compressString(const std::string& str, int level = 6);
    
    // 解压字符串 (一行代码)
    static std::string decompressString(const std::vector<uint8_t>& compressed);
    
    // === 文件操作 ===
    
    // 压缩文件 (一行代码)
    static void compressFile(const std::string& inputPath, const std::string& outputPath, int level = 6);
    
    // 解压文件 (一行代码)
    static void decompressFile(const std::string& inputPath, const std::string& outputPath);
    
    // === 流式操作 ===
    
    // 流式压缩 (处理大文件)
    static void compressStream(std::istream& input, std::ostream& output, int level = 6);
    
    // 流式解压 (处理大文件)
    static void decompressStream(std::istream& input, std::ostream& output);
    
    // === 实用工具 ===
    
    // 检查是否为ZIP格式
    static bool isZipFormat(const std::vector<uint8_t>& data);
    
    // 获取压缩率
    static double compressionRatio(size_t originalSize, size_t compressedSize);
};

#endif // ZIP_H