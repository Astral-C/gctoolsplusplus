#include "Archive.hpp"
#include "Util.hpp"
#include <algorithm>
#include <map>

namespace Archive {

uint16_t Hash(std::string str){
    uint16_t hash = 0;

    for (size_t i = 0; i < str.size(); i++){
        hash = (uint16_t)(hash * 3);
        hash += (uint16_t)str[i];
        hash &= 0xFFFF;
    }
    
    return hash;
}

///
/// Folder
///

std::shared_ptr<File> Folder::GetFile(std::filesystem::path path) {
    if(path.begin() == path.end()) return nullptr;


    for(auto file : mFiles){
        if(file->GetName() == path.begin()->string()){
            if(((++path.begin()) == path.end())){
                return file;
            } else {
                if(file->MountAsArchive()){
                    std::filesystem::path subPath;
                    for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
                    return (*file)->GetFile(subPath);
                }
            }
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

///
/// File
///

bool File::MountAsArchive(){
    std::shared_ptr<Rarc> arc =  Rarc::Create();

    bStream::CMemoryStream stream(mData, mSize, bStream::Endianess::Big, bStream::OpenMode::In);

    if(arc->Load(&stream)){
        mMountedArchive = arc;
        return true;
    } else {
        mMountedArchive = nullptr;
        return false; 
    }

}


///
/// Archive
///

std::map<std::string, uint32_t> Rarc::CalculateArchiveSizes(){
    uint32_t size = 0x40;
    
    std::map<std::string, bool> isUniqueStr;

    uint32_t dirEntrySize = 0x00; //??
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
            fileDataSize += file->GetSize();
        }
    }

    size += Util::PadTo32(dirEntrySize) + Util::PadTo32(fileEntrySize) + fileDataSize + Util::PadTo32(strTableSize);

    return {{"total", Util::PadTo32(size)}, {"dirEntries", Util::PadTo32(dirEntrySize)}, {"fileEntries", Util::PadTo32(fileEntrySize)}, {"fileData", fileDataSize}, {"strTable", Util::PadTo32(strTableSize)}};
}


void Rarc::SaveToFile(std::filesystem::path path, Compression::Format compression, uint8_t compressionLevel){

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
                if(ch >= folderName.size()) break;
                temp[ch] = toupper(folderName[ch]);
            }
            dirStream.writeBytes((uint8_t*)temp, 4);
        }

        dirStream.writeUInt32(stringTable[folderName]); // ??????
        dirStream.writeUInt16(Hash(folderName));
        dirStream.writeUInt16(folder->GetFileCount() + 2);
        dirStream.writeUInt32(currentFileIndex);

        // Write File entries
        for(auto file : folder->GetFiles()){
            fileStream.writeUInt16(currentFileIndex);
            fileStream.writeUInt16(Hash(file->GetName()));
            fileStream.writeUInt8(0x01 | 0x10);
            fileStream.writeUInt8(0x00);

            std::string fileName = file->GetName();
            fileStream.writeUInt16(stringTable[fileName]);
            fileStream.writeUInt32(fileDataStream.tell());
            fileStream.writeUInt32(file->GetSize());
            fileStream.writeUInt32(0x00);

            fileDataStream.writeBytes(file->GetData(), file->GetSize());

            currentFileIndex++;
        }

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
    }

    uint32_t totalArchiveSize = 0x40 + (mDirectories.size() * 0x10) + (currentFileIndex * 0x14);

    // Write Header
    headerStream.writeUInt32(0x52415243);
    headerStream.writeUInt32(archiveSizes["total"]);
    headerStream.writeUInt32(fileSystemChunk - archiveData);
    headerStream.writeUInt32(fileDataChunk - fileSystemChunk);
    headerStream.writeUInt32(fileDataStream.getSize());
    headerStream.writeUInt32(fileDataStream.getSize()); //TODO: mram flag!
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

    switch(compression){
        case Compression::Format::None:
            {
                bStream::CFileStream outFile(path.string(), bStream::Endianess::Big, bStream::OpenMode::Out);
                outFile.writeBytes(archiveData, archiveSizes["total"]);
            }
            break;
        
        case Compression::Format::YAY0:
            {
                bStream::CMemoryStream archiveOut(archiveData, archiveSizes["total"], bStream::Endianess::Big, bStream::OpenMode::In);
                bStream::CFileStream outFile(path.string(), bStream::Endianess::Big, bStream::OpenMode::Out);

                Compression::Yay0::Compress(&archiveOut, &outFile);
            }
            break; 
        case Compression::Format::YAZ0:
            {
                bStream::CMemoryStream archiveOut(archiveData, archiveSizes["total"], bStream::Endianess::Big, bStream::OpenMode::In);
                bStream::CFileStream outFile(path.string(), bStream::Endianess::Big, bStream::OpenMode::Out);

                Compression::Yaz0::Compress(&archiveOut, &outFile, compressionLevel);
            }
            break; 
    }


    delete[] archiveData;
}

// Needs error checking
bool Rarc::Load(bStream::CStream* stream){
    bStream::CMemoryStream decompressedArcStream(1, bStream::Endianess::Big, bStream::OpenMode::Out);
    
    bStream::CStream* rarcStream = stream;
    
    uint32_t magic = stream->readUInt32();

    if(magic == 1499560496){
        decompressedArcStream.Reserve(Compression::GetDecompressedSize(stream));
        Compression::Yaz0::Decompress(stream, &decompressedArcStream);
        decompressedArcStream.changeMode(bStream::OpenMode::In);    
        rarcStream = &decompressedArcStream;
    } else if(magic == 1499560240){
        decompressedArcStream.Reserve(Compression::GetDecompressedSize(stream));
        Compression::Yay0::Decompress(stream, &decompressedArcStream);
        decompressedArcStream.changeMode(bStream::OpenMode::In);
        rarcStream = &decompressedArcStream;
    }

    rarcStream->seek(0);

    magic = rarcStream->readUInt32();

    if(magic != 1380012611){
        return false;
    }
    
    
    rarcStream->seek(8); // Skip unneeded stuff

    uint32_t fsOffset = rarcStream->readUInt32();
    uint32_t fsSize = rarcStream->readUInt32();

    rarcStream->seek(fsOffset);

    uint32_t dirCount = rarcStream->readUInt32();
    uint32_t dirOffset = rarcStream->readUInt32();

    uint32_t fileCount = rarcStream->readUInt32();
    uint32_t fileOffset = rarcStream->readUInt32();

    uint32_t strTableSize = rarcStream->readUInt32();
    uint32_t strTableOffset = rarcStream->readUInt32();

    for(size_t i = 0; i < dirCount; i++) mDirectories.push_back(Folder::Create(GetPtr()));
 
    rarcStream->seek(dirOffset + fsOffset);
    for(size_t i = 0; i < dirCount; i++)
    {
        std::shared_ptr<Folder> folder = mDirectories[i];

        rarcStream->skip(4);
        uint32_t nameOffset = rarcStream->readUInt32();
        rarcStream->skip(2);

        uint16_t filenum = rarcStream->readUInt16();
        uint32_t fileoff = rarcStream->readUInt32();

        size_t readReturn = rarcStream->tell();
        
        rarcStream->seek(strTableOffset + fsOffset + nameOffset);
        char c;
        std::string dirName;
		while((c = rarcStream->readUInt8()) != '\0' && rarcStream->tell()  < (strTableOffset + fsOffset + strTableSize)){
			dirName.push_back(c);
		}

        folder->SetName(dirName);

        rarcStream->seek(readReturn);

        size_t pos = rarcStream->tell();
        rarcStream->seek(fileOffset + (fileoff * 0x14) + fsOffset);

        for(size_t f = 0; f < filenum; f++)
        {
            std::shared_ptr<File> file = File::Create();

            uint16_t id = rarcStream->readUInt16();
	        rarcStream->skip(2);

            uint32_t fileAttrs = rarcStream->readUInt32();
            uint32_t fileNameOffset = fileAttrs & 0x00FFFFFF;
            uint8_t attr = (fileAttrs >> 24) & 0xFF;
            uint32_t start = rarcStream->readUInt32();
            
            uint32_t fileSize = rarcStream->readUInt32();

            size_t readReturn = rarcStream->tell();
            
            rarcStream->seek(strTableOffset + fsOffset + fileNameOffset);

            char c;
            std::string name;
            while((c = rarcStream->readUInt8()) != '\0' && rarcStream->tell()  < (strTableOffset + fsOffset + strTableSize)){
                name.push_back(c);
            }

            rarcStream->seek(readReturn);
            
            rarcStream->skip(4);

            if(attr & 0x01){
                
                unsigned char* fileData = new unsigned char[fileSize];
                
                size_t pos = rarcStream->tell();
                rarcStream->seek(fsOffset + fsSize + start);
                rarcStream->readBytesTo(fileData, fileSize);
                rarcStream->seek(pos);

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
        rarcStream->seek(pos);
    }
    return true;
}
}