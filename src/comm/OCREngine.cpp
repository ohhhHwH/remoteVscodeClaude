#include "comm/IOCR.h"
#include <windows.h>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <filesystem>

class OCREngine : public IOCR {
public:
    std::string Recognize(const cv::Mat& image) override {
        if (image.empty()) return "";

        // 保存临时图片，调用Windows PowerShell OCR
        std::string tempPath = std::filesystem::temp_directory_path().string() + "\\ocr_temp.png";
        cv::imwrite(tempPath, image);

        // 通过PowerShell调用Windows.Media.Ocr
        std::string cmd = "powershell -Command \""
            "Add-Type -AssemblyName System.Runtime.WindowsRuntime;"
            "$null = [Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime];"
            "$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages();"
            "$file = [Windows.Storage.StorageFile]::GetFileFromPathAsync('" + tempPath + "').GetAwaiter().GetResult();"
            "$stream = $file.OpenAsync([Windows.Storage.FileAccessMode]::Read).GetAwaiter().GetResult();"
            "$decoder = [Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream).GetAwaiter().GetResult();"
            "$bitmap = $decoder.GetSoftwareBitmapAsync().GetAwaiter().GetResult();"
            "$result = $engine.RecognizeAsync($bitmap).GetAwaiter().GetResult();"
            "Write-Output $result.Text"
            "\" 2>nul";

        std::string result;
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            _pclose(pipe);
        }

        // 去除末尾换行
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        std::filesystem::remove(tempPath);
        return result;
    }
};
