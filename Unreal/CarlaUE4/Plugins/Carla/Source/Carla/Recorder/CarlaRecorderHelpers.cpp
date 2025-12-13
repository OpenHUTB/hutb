// Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include <fstream>
#include <vector>

#include "UnrealString.h"
#include "CarlaRecorderHelpers.h"

// create a temporal buffer to convert from and to FString and bytes
static std::vector<uint8_t> CarlaRecorderHelperBuffer;

// get the final path + filename
std::string GetRecorderFilename(std::string Filename)
{
  std::string Filename2;

  // check if a relative path was specified
  if (Filename.find("\\") != std::string::npos || Filename.find("/") != std::string::npos || Filename.find(":") != std::string::npos)
    Filename2 = Filename;
  else
  {
    FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    Filename2 = TCHAR_TO_UTF8(*Path) + Filename;
  }

  return Filename2;
}

// ------
// write
// ------

// write binary data from FVector
void WriteFVector(std::ostream &OutFile, const FVector &InObj)
{
  WriteValue<float>(OutFile, InObj.X);
  WriteValue<float>(OutFile, InObj.Y);
  WriteValue<float>(OutFile, InObj.Z);
}

// write binary data from FTransform
void WriteFTransform(std::ofstream &OutFile, const FTransform &InObj)
{
  WriteFVector(OutFile, InObj.GetTranslation());
  WriteFVector(OutFile, InObj.GetRotation().Euler());
}

// write binary data from FRotator
void WriteFRotator(std::ofstream &OutFile, const FRotator &InObj)
{
  WriteValue<float>(OutFile, InObj.Pitch);
  WriteValue<float>(OutFile, InObj.Roll);
  WriteValue<float>(OutFile, InObj.Yaw);
}

// write binary data from FVector2D
void WriteFVector2D(std::ofstream &OutFile, const FVector2D &InObj)
{
  WriteValue<float>(OutFile, InObj.X);
  WriteValue<float>(OutFile, InObj.Y);
}

// write binary data to FLinearColor
void WriteFLinearColor(std::ofstream &InFile, const FLinearColor &InObj)
{
  WriteValue<float>(InFile, InObj.A);
  WriteValue<float>(InFile, InObj.B);
  WriteValue<float>(InFile, InObj.G);
  WriteValue<float>(InFile, InObj.R);
}

// write binary data from FTransform
// void WriteFTransform(std::ofstream &OutFile, const FTransform &InObj){
// WriteFVector(OutFile, InObj.GetTranslation());
// WriteFVector(OutFile, InObj.GetRotation().Euler());
// }

// write binary data from FString (length + text)
void WriteFString(std::ostream &OutFile, const FString &InObj)
{
  // encode the string to UTF8 to know the final length
  FTCHARToUTF8 EncodedString(*InObj);
  int16_t Length = EncodedString.Length();
  // write
  WriteValue<uint16_t>(OutFile, Length);
  OutFile.write(reinterpret_cast<char *>(TCHAR_TO_UTF8(*InObj)), Length);
}

// -----
// read
// -----

// read binary data to FVector
void ReadFVector(std::istream &InFile, FVector &OutObj)
{
  ReadValue<float>(InFile, OutObj.X);
  ReadValue<float>(InFile, OutObj.Y);
  ReadValue<float>(InFile, OutObj.Z);
}

// read binary data to FTransform
void ReadFTransform(std::ifstream &InFile, FTransform &OutObj)
{
  FVector Vec;
  ReadFVector(InFile, Vec);
  OutObj.SetTranslation(Vec);
  ReadFVector(InFile, Vec);
  OutObj.GetRotation().MakeFromEuler(Vec);
}

// read binary data to FRotator
void ReadFRotator(std::ifstream &InFile, FRotator &OutObj)
{
  ReadValue<float>(InFile, OutObj.Pitch);
  ReadValue<float>(InFile, OutObj.Roll);
  ReadValue<float>(InFile, OutObj.Yaw);
}

// read binary data to FVector2D
void ReadFVector2D(std::ifstream &InFile, FVector2D &OutObj)
{
  ReadValue<float>(InFile, OutObj.X);
  ReadValue<float>(InFile, OutObj.Y);
}

// read binary data to FLinearColor
void ReadFLinearColor(std::ifstream &InFile, FLinearColor &OutObj)
{
  ReadValue<float>(InFile, OutObj.A);
  ReadValue<float>(InFile, OutObj.B);
  ReadValue<float>(InFile, OutObj.G);
  ReadValue<float>(InFile, OutObj.R);
}

// read binary data to FTransform
// void ReadFTransform(std::ifstream &InFile, FTransform &OutObj){
// FVector Vec;
// ReadFVector(InFile, Vec);
// OutObj.SetTranslation(Vec);
// ReadFVector(InFile, Vec);
// OutObj.GetRotation().MakeFromEuler(Vec);
// }

// read binary data to FString (length + text)
void ReadFString(std::istream &InFile, FString &OutObj)
{
  uint16_t Length;
  ReadValue<uint16_t>(InFile, Length);
  // make room in vector buffer
  if (CarlaRecorderHelperBuffer.capacity() < Length + 1)
  {
    CarlaRecorderHelperBuffer.reserve(Length + 1);
  }
  CarlaRecorderHelperBuffer.clear();
  // initialize the vector space with 0
  CarlaRecorderHelperBuffer.resize(Length + 1);
  // read
  InFile.read(reinterpret_cast<char *>(CarlaRecorderHelperBuffer.data()), Length);
  // convert from UTF8 to FString
  OutObj = FString(UTF8_TO_TCHAR(CarlaRecorderHelperBuffer.data()));
}
