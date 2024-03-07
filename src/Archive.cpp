#include "Archive.hpp"
#include <algorithm>
#include <map>

#define calcPad32(x) ((x + (32-1)) & ~(32-1))

namespace Archive {

const char ROOT_ID[5] = "ROOT";

void padTo32(bStream::CStream* stream, size_t size){
    for(int i = 0; i < calcPad32(size); i++) stream->writeUInt8(0);
}

uint16_t Hash(std::string str){
    uint16_t hash = 0;

    for (size_t i = 0; i < str.size(); i++){
        hash = (uint16_t)(hash * 3);
        hash += (uint16_t)str[i];
        hash &= 0xFFFF;
    }
    
    return hash;
}


std::map<std::string, uint32_t> Rarc::CalculateArchiveSizes(){
    uint32_t size = 0x40;
    
    std::map<std::string, bool> isUniqueStr;

    uint32_t dirEntrySize = 0;
    uint32_t fileEntrySize = 0;
    uint32_t fileDataSize = 0;
    uint32_t strTableSize = 5; // Accounts for .\0 and ..\0 strs

    for(auto dir : mDirectories){
        dirEntrySize += 0x10;
        
        if(isUniqueStr.count(dir->GetName()) == 0){
            strTableSize += dir->GetName().size() + 1;
            isUniqueStr.insert({dir->GetName(), true});
        } 
        
        fileEntrySize += 0x14 + 0x14; // . and .. entries
        
        for(auto subdir : dir->GetSubdirectories()){
            fileEntrySize += 0x14;
        }

        for(auto file : dir->GetFiles()){
            if(isUniqueStr.count(file->GetName()) == 0){
                strTableSize += file->GetName().size() + 1;
                isUniqueStr.insert({file->GetName(), true});
            } 

            fileEntrySize += 0x14;
            fileDataSize += calcPad32(file->GetSize());
        }
    }

    size += dirEntrySize + fileEntrySize + fileDataSize + calcPad32(strTableSize);

    return {{"total", calcPad32(size)}, {"dirEntries", dirEntrySize}, {"fileEntries", fileEntrySize}, {"fileData", fileDataSize}, {"strTable", calcPad32(strTableSize)}};
}


///
/// Folder
///

std::shared_ptr<File> Folder::GetFile(std::filesystem::path path) {
    if(path.begin() == path.end()) return nullptr;


    for(auto file : mFiles){
        if(file->GetName() == path.begin()->string() && ((++path.begin()) == path.end())){
            return file;
        }
    }

    std::shared_ptr<File> file = nullptr;

    for(auto dir : mFolders){
        if(dir->GetName() == path.begin()->string()){
            std::filesystem::path subPath;
            for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
            file = dir->GetFile(subPath);
        }
    }

    return file;
}

std::shared_ptr<Folder> Folder::GetFolder(std::filesystem::path path){
    for(auto dir : mFolders){
        if(dir->GetName() == path.begin()->string() && ((++path.begin()) == path.end())){
            return dir;
        } else if(dir->GetName() == path.begin()->string()) {
            std::filesystem::path subPath;
            for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
            return dir->GetFolder(subPath);
        }
    }

    return nullptr;
}

std::shared_ptr<Folder> Folder::Copy(std::shared_ptr<Rarc> archive){
    std::shared_ptr<Folder> newFolder = Folder::Create(archive);
    newFolder->SetName(mName);
    for(auto dir : mFolders){
        newFolder->AddSubdirectory(dir);
    }

    for(auto file : mFiles){
        newFolder->AddFile(file);
    }
    return newFolder;
}

void Folder::AddSubdirectory(std::shared_ptr<Folder> dir){
    if(dir->GetArchive() != mArchive){
        std::shared_ptr<Folder> copy = dir->Copy(mArchive);
        AddSubdirectory(copy);
    } else {
        dir->SetParentUnsafe(GetPtr());
        mFolders.push_back(dir);

        auto dirIter = std::find(mArchive->mDirectories.begin(), mArchive->mDirectories.end(), dir);
        if(dirIter == mArchive->mDirectories.end()){
            mArchive->mDirectories.push_back(dir); 
        }
    }
}

void Rarc::SaveToFile(std::string path){

    std::map<std::string, uint32_t> archiveSizes = CalculateArchiveSizes();


    uint8_t* archiveData = new uint8_t[archiveSizes["total"]];
    memset(archiveData, 0, archiveSizes["total"]);
    
    uint8_t* fileSystemChunk = archiveData + 0x20;
    uint8_t* dirChunk = archiveData + 0x40;
    uint8_t* fileChunk = archiveData + 0x40 + archiveSizes["dirEntries"];
    uint8_t* strTableChunk = archiveData + 0x40 + archiveSizes["dirEntries"] + archiveSizes["fileEntries"];
    uint8_t* fileDataChunk = archiveData + 0x40 + archiveSizes["dirEntries"] + archiveSizes["fileEntries"] + archiveSizes["strTable"];
    
    bStream::CMemoryStream headerStream(archiveData, 0x20, bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream fileSystemStream(fileSystemChunk, 0x20, bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream dirStream(dirChunk, archiveSizes["dirEntries"], bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream fileStream(fileChunk, archiveSizes["fileEntries"], bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream stringTableStream(strTableChunk, archiveSizes["strTable"], bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream fileDataStream(fileDataChunk, archiveSizes["fileData"], bStream::Endianess::Big, bStream::OpenMode::Out);

    // Generate String Table

    std::map<std::string, uint32_t> stringTable{{".", 0x00}, {"..", 0x02}};
    
    stringTableStream.writeString(".");
    stringTableStream.writeUInt8(0x00);
    stringTableStream.writeString("..");
    stringTableStream.writeUInt8(0x00);

    for(auto dir : mDirectories){
        if(stringTable.count(dir->GetName()) == 0){
            stringTable.insert({dir->GetName(), stringTableStream.tell()});
            stringTableStream.writeString(dir->GetName());
            stringTableStream.writeUInt8(0x00);
        }
        for(auto file : dir->GetFiles()){
            if(stringTable.count(file->GetName()) == 0){
                stringTable.insert({file->GetName(), stringTableStream.tell()});
                stringTableStream.writeString(file->GetName());
                stringTableStream.writeUInt8(0x00);
            }
        }
    }

    size_t currentFileIndex = 0;

    // Write Archive Structure
    for(size_t i = 0; i < mDirectories.size(); i++)
    {
        std::shared_ptr<Folder> folder = mDirectories[i];

        std::string folderName = folder->GetName();

        //Write IDs
        if(i == 0){
            dirStream.writeUInt32(0x524F4F54);
        } else {
            char temp[4];
            memset(temp, 0x20, sizeof(temp));
            for (size_t ch = 0; ch < 4; ch++){
                temp[ch] = toupper(folderName[ch]);
            }
            dirStream.writeBytes(temp, 4);
        }

        dirStream.writeUInt32(stringTable[folderName]); // ??????
        dirStream.writeUInt16(Hash(folderName));
        dirStream.writeUInt16(folder->GetFileCount() + 2);
        dirStream.writeUInt32(currentFileIndex);

        // Write .
        // [v]: perhaps this should be put into a function of some kind? unsure...
        fileStream.writeUInt16(0xFFFF);
        fileStream.writeUInt16(Hash("."));
        fileStream.writeUInt8(0x02);
        fileStream.writeUInt8(0x00);
        fileStream.writeUInt16(0x00);
        fileStream.writeUInt32(i);
        fileStream.writeUInt32(0x10);
        fileStream.writeUInt32(0x00);
        
        // And write ..
        fileStream.writeUInt16(0xFFFF);
        fileStream.writeUInt16(Hash(".."));
        fileStream.writeUInt8(0x02);
        fileStream.writeUInt8(0x00);
        fileStream.writeUInt16(0x02);
        if(folder->GetParent() != nullptr){
            auto dirIter = std::find(mDirectories.begin(), mDirectories.end(), folder->GetParent());
            fileStream.writeUInt32(dirIter - mDirectories.begin());
        } else {
            fileStream.writeUInt32((uint32_t)-1);
        }
        fileStream.writeUInt32(0x10);
        fileStream.writeUInt32(0x00);

        currentFileIndex += 2; // account for . and .. file entries

        // Write Subdirectory entries
        for(auto subdir : folder->GetSubdirectories()){
            auto subdirIter = std::find(mDirectories.begin(), mDirectories.end(), subdir);
            fileStream.writeUInt16(0xFFFF);
            fileStream.writeUInt16(Hash(subdir->GetName()));
            fileStream.writeUInt8(0x02);
            fileStream.writeUInt8(0x00);

            std::string subdirName = subdir->GetName();

            fileStream.writeUInt16(stringTable[subdirName]); // ????
            fileStream.writeUInt32(subdirIter - mDirectories.begin());
            fileStream.writeUInt32(0x10);
            fileStream.writeUInt32(0x00);
            currentFileIndex++;
        }

        // Write File entries
        for(auto file : folder->GetFiles()){
            fileStream.writeUInt16(currentFileIndex);
            fileStream.writeUInt16(Hash(file->GetName()));
            fileStream.writeUInt8(0x01);
            fileStream.writeUInt8(0x00);

            std::string fileName = file->GetName();
            fileStream.writeUInt16(stringTable[fileName]);
            fileStream.writeUInt32(fileDataStream.tell());
            fileStream.writeUInt32(file->GetSize());
            fileStream.writeUInt32(0x00);

            fileDataStream.writeBytes((char*)file->GetData(), file->GetSize());
            padTo32(&fileDataStream, fileDataStream.getSize());

            currentFileIndex++;
        }
    }

    uint32_t totalArchiveSize = 0x40 + (mDirectories.size() * 0x10) + (currentFileIndex * 0x14);

    // Write Header
    headerStream.writeUInt32(0x52415243);
    headerStream.writeUInt32(archiveSizes["total"]);
    headerStream.writeUInt32(fileSystemChunk - archiveData);
    headerStream.writeUInt32(fileDataChunk - fileSystemChunk);
    headerStream.writeUInt32(fileDataStream.getSize());
    headerStream.writeUInt32(0); //TODO: mram flag!
    headerStream.writeUInt32(0); //TODO: aram flag!
    headerStream.writeUInt32(0); // pad

    //Write FS Header
    fileSystemStream.writeUInt32(mDirectories.size());
    fileSystemStream.writeUInt32(dirChunk - fileSystemChunk);
    fileSystemStream.writeUInt32(currentFileIndex);
    fileSystemStream.writeUInt32(fileChunk - fileSystemChunk);
    fileSystemStream.writeUInt32(stringTableStream.getSize());
    fileSystemStream.writeUInt32(strTableChunk - fileSystemChunk);
    fileSystemStream.writeUInt16(currentFileIndex);
    fileSystemStream.writeUInt8(0);
    fileSystemStream.writeUInt8(0);
    fileSystemStream.writeUInt32(0);

    bStream::CFileStream outFile(path, bStream::Endianess::Big, bStream::OpenMode::Out);
    outFile.writeBytes((char*)archiveData, archiveSizes["total"]);

    delete[] archiveData;
}

void Rarc::Load(bStream::CStream* stream){
    stream->seek(8); // Skip unneeded stuff

    uint32_t fsOffset = stream->readUInt32();
    uint32_t fsSize = stream->readUInt32();

    stream->seek(fsOffset);

    uint32_t dirCount = stream->readUInt32();
    uint32_t dirOffset = stream->readUInt32();

    uint32_t fileCount = stream->readUInt32();
    uint32_t fileOffset = stream->readUInt32();

    uint32_t strTableSize = stream->readUInt32();
    uint32_t strTableOffset = stream->readUInt32();

    for(size_t i = 0; i < dirCount; i++) mDirectories.push_back(Folder::Create(GetPtr()));
 
    stream->seek(dirOffset + fsOffset);
    for(size_t i = 0; i < dirCount; i++)
    {
        std::shared_ptr<Folder> folder = mDirectories[i];

        stream->skip(4);
        uint32_t nameOffset = stream->readUInt32();
        stream->skip(2);

        uint16_t filenum = stream->readUInt16();
        uint32_t fileoff = stream->readUInt32();

        size_t readReturn = stream->tell();
        
        stream->seek(strTableOffset + fsOffset + nameOffset);
        char c;
        std::string dirName;
		while((c = stream->readUInt8()) != '\0' && stream->tell()  < (strTableOffset + fsOffset + strTableSize)){
			dirName.push_back(c);
		}

        folder->SetName(dirName);

        stream->seek(readReturn);

        size_t pos = stream->tell();
        stream->seek(fileOffset + (fileoff * 0x14) + fsOffset);

        for(size_t f = 0; f < filenum; f++)
        {
            std::shared_ptr<File> file = File::Create();

            uint16_t id = stream->readUInt16();
	        stream->skip(2);

            uint32_t fileAttrs = stream->readUInt32();
            uint32_t fileNameOffset = fileAttrs & 0x00FFFFFF;
            uint8_t attr = (fileAttrs >> 24) & 0xFF;
            uint32_t start = stream->readUInt32();
            
            uint32_t fileSize = stream->readUInt32();

            size_t readReturn = stream->tell();
            
            stream->seek(strTableOffset + fsOffset + fileNameOffset);

            char c;
            std::string name;
            while((c = stream->readUInt8()) != '\0' && stream->tell()  < (strTableOffset + fsOffset + strTableSize)){
                name.push_back(c);
            }

            stream->seek(readReturn);
            
            stream->skip(4);

            if(attr & 0x01){
                
                unsigned char* fileData = new unsigned char[fileSize];
                
                size_t pos = stream->tell();
                stream->seek(fsOffset + fsSize + start);
                stream->readBytesTo(fileData, fileSize);
                stream->seek(pos);

                file->SetName(name);
                file->SetData(fileData, fileSize);
                folder->AddFile(file);
                
                delete[] fileData;                
            } else if(attr & 0x02) {
                if(start != -1 && name != ".." && name != "."){
                    folder->AddSubdirectory(mDirectories[start]);
                }
            }

        }
        stream->seek(pos);
    }
}
}