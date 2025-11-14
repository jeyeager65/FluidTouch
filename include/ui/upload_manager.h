#ifndef UPLOAD_MANAGER_H
#define UPLOAD_MANAGER_H

#include <Arduino.h>
#include <functional>

class UploadManager {
public:
    using ProgressCallback = std::function<void(size_t current, size_t total)>;
    using CompleteCallback = std::function<void(bool success, const char* error)>;

    static bool init();
    static bool uploadFile(const char* localPath, 
                          const char* filename,
                          ProgressCallback onProgress,
                          CompleteCallback onComplete);
    static bool isUploading();

private:
    static bool _uploading;
};

#endif // UPLOAD_MANAGER_H
