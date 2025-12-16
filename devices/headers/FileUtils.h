/*
 * FileUtils.h
 *
 *  Created on: Apr 3, 2015
 *      Author: Francesco Guatieri
 */

#ifndef SOURCE_FILEUTILS_H_
#define SOURCE_FILEUTILS_H_

#include <dirent.h>
#include <sys/stat.h>
// #include <sys/sendfile.h>
#include <pwd.h>

#include "Error.h"

// #include "Time.h"
#include "StringUtils.h"

template <class T> T FileError(string Error, T Return = 0) {
  Shout(Error);
  return Return;
}

string EscapeShellParameter(string Param) {
  return "\"" + StringReplace(Param, "\"", "\\\"") + "\"";
}

inline bool FileExists(const string &name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

inline bool FolderExists(const string &name) {
  struct stat st;
  if (stat(name.c_str(), &st) == 0)
    return ((st.st_mode & S_IFDIR) != 0);

  return false;
}

inline int GetInode(const string &name) {
  struct stat buffer;
  if (stat(name.c_str(), &buffer))
    return -1;
  return buffer.st_ino;
}

bool CreateFolder(string FolderName, bool AutoParents = true) {
  if (!FolderName.size())
    return Shout("Empty folder name provided for folder creation.");

  if (!AutoParents)
    return !mkdir(FolderName.c_str(), 0755);

  vector<string> Splitted = StringSplit(FolderName, '/');
  string Path;

  for (size_t i = 0; i < Splitted.size(); ++i) {
    if (i)
      Path.push_back('/');
    Path += Splitted[i];
    if (Path.size() && !FolderExists(Path))
      if (mkdir(Path.c_str(), 0755))
        return false;
  };

  return true;
}

size_t FileSize(string FileName) {
  struct stat statbuf;
  if (stat(FileName.c_str(), &statbuf) == -1)
    return ("Unable to stat size of file \"" + FileName + "\"\n", -1);

  return statbuf.st_size;
}

time_t FileModificationTime(string FileName) {
  struct stat statbuf;
  if (stat(FileName.c_str(), &statbuf) == -1)
    return ("Unable to stat size of file \"" + FileName + "\"\n", -1);

  return statbuf.st_mtime;
}

time_t FileAccessTime(string FileName) {
  struct stat statbuf;
  if (stat(FileName.c_str(), &statbuf) == -1)
    return ("Unable to stat size of file \"" + FileName + "\"\n", -1);

  return statbuf.st_atime;
}

time_t FileCreationTime(string FileName) {
  struct stat statbuf;
  if (stat(FileName.c_str(), &statbuf) == -1)
    return ("Unable to stat size of file \"" + FileName + "\"\n", -1);

  return statbuf.st_ctime;
}

bool DeleteFile(string FileName) { return !remove(FileName.c_str()); }

bool SetFileOwner(string FileName, string UserName) {
  struct passwd *PwdEntry = getpwnam(UserName.c_str());
  if (PwdEntry == nullptr)
    return false;
  return !chown(FileName.c_str(), PwdEntry->pw_uid, -1);
}

bool SetFileGroup(string FileName, string UserName) {
  struct passwd *PwdEntry = getpwnam(UserName.c_str());
  if (PwdEntry == nullptr)
    return false;
  return !chown(FileName.c_str(), -1, PwdEntry->pw_gid);
}

bool SetFilePermission(string FileName, int OctalFlags) {
  return !chmod(FileName.c_str(), OctalFlags & 0777);
}

bool RenameFile(string Origin, string Destination, bool Overwrite = true) {
  if (!Overwrite && FileExists(Destination))
    return Shout("Cannot rename file: destination already exists.", false);

  return !rename(Origin.c_str(), Destination.c_str());
}

bool CreateSymbolicLink(const string &From, const string &To) {
  return !symlink(To.c_str(), From.c_str());
}

bool CopyFile(string Origin, string Destination, bool Overwrite = true) {
  if (!Overwrite && FileExists(Destination))
    return Shout("Cannot copy file: destination already exists.", false);

  size_t BytesToCopy = FileSize(Origin);

  int OriFD = open(Origin.c_str(), O_RDONLY);
  int DestFD = open(Destination.c_str(), O_WRONLY | O_CREAT, 0600);

  bool TempRes = true;

#if defined(__APPLE__)
  off_t offset = 0;
  off_t len = BytesToCopy;
  // sendfile on macOS: int sendfile(int fd, int s, off_t offset, off_t *len, struct sf_hdtr *hdtr, int flags);
  int result = sendfile(OriFD, DestFD, offset, &len, nullptr, 0);
  if (result == -1 || len != BytesToCopy) {
    TempRes = false;
  }
#else
  while (BytesToCopy != 0) {
    size_t Chunck = sendfile(DestFD, OriFD, nullptr, BytesToCopy);
    if (Chunck == 0) {
      TempRes = false;
      break;
    };

    if (Chunck > BytesToCopy) {
      Warn("Suspicious chunk size (" + itos(Chunck) + "/" + itos(BytesToCopy) +
           ") from Sendfile call.");
      break;
    };

    BytesToCopy -= Chunck;
  };
#endif

  close(OriFD);
  close(DestFD);

  return TempRes;
}

inline bool RenameFolder(string Origin, string Destination) {
  return RenameFile(Origin, Destination);
}

bool WriteFile(string FileName, string Content) {
  ofstream File(FileName.c_str(), ios::out);

  if (!File)
    return FileError("Unable to open file \"" + FileName + "\" for output.\n",
                     false);

  File << Content;
  File.close();
  return true;
}

bool WriteFile(string FileName, vector<uint8_t> Content) {
  ofstream File(FileName.c_str(), ios::out | ios::binary);

  if (!File)
    return FileError("Unable to open file \"" + FileName + "\" for output.\n",
                     false);

  File.write((char *)&(Content[0]), Content.size());
  File.close();
  return true;
}

bool AppendToFile(string FileName, string Content) {
  ofstream File(FileName.c_str(), ios::out | ios::app);

  if (!File)
    return FileError("Unable to open file \"" + FileName + "\" for output.\n",
                     false);

  File << Content;
  File.flush();
  File.close();
  return true;
}

string ReadFile(const string &FileName) {
  ifstream File(FileName.c_str(), ios::binary);

  if (!File)
    return FileError("Error! Unable to open file \"" + FileName + "\"\n", "");

  string TempRes;

  File.seekg(0, ios::end);
  TempRes.reserve(File.tellg());
  File.seekg(0, ios::beg);

  TempRes.assign((istreambuf_iterator<char>(File)),
                 istreambuf_iterator<char>());
  return TempRes;
}

bool ReadFile(const string &FileName, string &Destination) {
  ifstream File(FileName.c_str(), ios::binary);

  if (!File) {
    Destination.clear();
    return Shout("Unable to open file \"" + FileName + "\"\n", false);
  }

  File.seekg(0, ios::end);
  Destination.reserve(File.tellg());
  File.seekg(0, ios::beg);

  Destination.assign((istreambuf_iterator<char>(File)),
                     istreambuf_iterator<char>());
  return true;
}

bool isRegularFile(string path) {
  struct stat path_stat;
  stat(path.c_str(), &path_stat);
  return S_ISREG(path_stat.st_mode);
}

bool isDir(string fPath) { return !isRegularFile(fPath); }

template <class T> bool VirtualSave(string &FileName, T &Caller) {
  ofstream File;

  File.open(FileName.c_str(), ios::out | ios::binary);
  if (!File)
    return FileError(
        "Error. Unable to open output file \"" + FileName + "\".\n", false);

  bool TempRes = Caller.Save(File);
  File.close();
  return TempRes;
}

template <class T> bool VirtualLoad(string &FileName, T &Caller) {
  ifstream File;

  File.open(FileName.c_str(), ios::in | ios::binary);
  if (!File)
    return FileError("Error. Unable to open input file \"" + FileName + "\".\n",
                     false);

  bool TempRes = Caller.Load(File);
  File.close();
  return TempRes;
}

enum FGDirEntityTypes {
  FGDirEntity_Uknown = 1,
  FGDirEntity_Fifo = 2,
  FGDirEntity_CharacterDevice = 4,
  FGDirEntity_Directory = 8,
  FGDirEntity_BlockDevice = 16,
  FGDirEntity_RegularFile = 32,
  FGDirEntity_Link = 64,
  FGDirEntity_Socket = 128,
  FGDirEntity_Whiteout = 256
};

int ConvertDirEntityType(int POSIXType) {
  if (POSIXType < 0 || POSIXType > 14)
    return FGDirEntity_Uknown;

  static const FGDirEntityTypes Converted[15] = {FGDirEntity_Uknown,
                                                 FGDirEntity_Fifo,
                                                 FGDirEntity_CharacterDevice,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_Directory,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_BlockDevice,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_RegularFile,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_Link,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_Socket,
                                                 FGDirEntity_Uknown,
                                                 FGDirEntity_Whiteout};

  return Converted[POSIXType];
}

// Type is defined as or of FGDirEntity_*s
vector<string> ListFolderContent(string FolderPath, int TypeMask = 0xFFFFFFFF) {
  DIR *Directory = opendir(FolderPath.c_str());
  if (Directory == nullptr)
    return vector<string>();

  vector<string> TempRes;

  struct dirent *Entity = readdir(Directory);

  while (Entity != nullptr) {
    if (ConvertDirEntityType(Entity->d_type) & TypeMask)
      TempRes.push_back(Entity->d_name);
    Entity = readdir(Directory);
  };

  closedir(Directory);

  return TempRes;
}

inline vector<string> ListFiles(string FolderPath) {
  return ListFolderContent(FolderPath, FGDirEntity_RegularFile);
}
inline vector<string> ListSubfolders(string FolderPath) {
  return ListFolderContent(FolderPath, FGDirEntity_Directory);
}

// Lists only files and directories, placing directories first, sorting
// alphabetically the entire content and marking directories by appending a '/'
// to them
vector<string> ListFolder(string FolderPath) {
  DIR *Directory = opendir(FolderPath.c_str());
  if (Directory == nullptr)
    return vector<string>();

  vector<string> TempResFiles;
  vector<string> TempRes;

  struct dirent *Entity = readdir(Directory);

  while (Entity != nullptr) {
    if (Entity->d_type == DT_REG)
      TempResFiles.push_back(Entity->d_name);
    else if (Entity->d_type == DT_DIR)
      TempRes.push_back(Entity->d_name + string("/"));
    Entity = readdir(Directory);
  };

  closedir(Directory);

  sort(TempRes.begin(), TempRes.end());
  sort(TempResFiles.begin(), TempResFiles.end());
  TempRes.insert(TempRes.end(), TempResFiles.begin(), TempResFiles.end());

  return TempRes;
}

class FGFolder {
public:
  string Name;
  string FullPath;
  vector<FGFolder> SubFolders;
  vector<string> Files;

  FGFolder(string DirPath, string DirName = "") {
    if (!DirPath.size() || DirPath[DirPath.size() - 1] != '/')
      DirPath.push_back('/');

    Name = DirName;
    FullPath = DirPath;

    Files = ListFiles(DirPath);
    vector<string> SubFolderNames = ListSubfolders(DirPath);

    sort(Files.begin(), Files.end());
    sort(SubFolderNames.begin(), SubFolderNames.end());

    SubFolders.reserve(SubFolderNames.size());
    for (auto &s : SubFolderNames)
      if (s != "." && s != "..")
        SubFolders.emplace_back(DirPath + s + "/", s);
  };

  void EnlistAllFiles(vector<string> *Result, string PartialPath = "") {
    for (auto &f : SubFolders)
      f.EnlistAllFiles(Result, PartialPath + f.Name + "/");
    for (auto &f : Files)
      Result->push_back(PartialPath + f);
  };

  void Dump(int Depth = 0) {
    for (auto &f : SubFolders) {
      cout << string(Depth, ' ') << f.Name << "\n";
      f.Dump(Depth + 3);
    };

    for (auto &f : Files)
      cout << string(Depth, ' ') << f << "\n";
  };

  bool RecursiveRemove() {
    for (auto &f : SubFolders)
      if (!f.RecursiveRemove())
        return false;

    for (auto &f : Files)
      if (!DeleteFile(FullPath + f))
        return false;

    if (rmdir(FullPath.c_str()))
      return false;

    return true;
  }

  bool RecursiveCopy(string NewPath, bool Overwrite, bool BreakOnError) {
    if (!NewPath.size())
      return Shout("Empty string provided as destintion for recursive copy!",
                   false);
    if (NewPath[NewPath.size() - 1] != '/')
      NewPath.push_back('/');

    if (!CreateFolder(NewPath, true) && BreakOnError)
      return false;

    for (auto &f : SubFolders)
      if (!f.RecursiveCopy(NewPath + f.Name, Overwrite, BreakOnError) &&
          BreakOnError)
        return false;

    for (auto &f : Files)
      if (!CopyFile(FullPath + f, NewPath + f, Overwrite) && BreakOnError)
        return false;

    return true;
  }
};

bool DeleteFolder(string FolderName, bool Recursive = true) {
  if (!Recursive) {
    return !rmdir(FolderName.c_str());
  };

  FGFolder Folder(FolderName);
  return Folder.RecursiveRemove();
}

bool CopyFolder(string Source, string Destination, bool Overwrite = false,
                bool BreakOnError = false) {
  FGFolder Folder(Source);
  return Folder.RecursiveCopy(Destination, Overwrite, BreakOnError);
}

#endif /* SOURCE_FILEUTILS_H_ */
