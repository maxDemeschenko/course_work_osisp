#pragma once

#include <string>
#include <list>
#include <chrono>
#include <uuid/uuid.h>

struct FileChange
{
    std::chrono::system_clock::time_point timestamp; 
    std::string changeType;                          
    std::string checksum;                            
    std::string savedVersionId;                      
    std::string user;                                
    std::string additionalInfo;                      
};

struct FileHistory
{
    std::list<FileChange> changes;
};

struct TrackingFile
{
    std::string filePath;      
    std::string fileId;        
    FileHistory history;       
    std::string lastChecksum;  
    bool isMissing = false;    
};



bool rollbackTo(const FileChange& change);

bool rollbackToVersion(const std::string& versionId);
