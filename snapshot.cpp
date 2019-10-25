#include <iostream>
#include <fstream>
#include <sstream>
#include "Hart.hpp"

 
using namespace WdRiscv;


template <typename URV>
bool
Hart<URV>::saveSnapshot(const std::string& regFile, const std::string& memFile)
{
  if (not saveSnapshotRegs(regFile))
    return false;
  if (not memory_.saveSnapshot(memFile))
    return false;
  return true;
}


template <typename URV>
bool
Hart<URV>::loadSnapshot(const std::string& regFile, const std::string& memFile)
{
  if (not loadSnapshotRegs(regFile))
    return false;
  if (not memory_.loadSnapshot(memFile))
    return false;
  return true;
}


template <typename URV>
bool
Hart<URV>::saveSnapshotRegs(const std::string & filename)
{
  // open file for write, check success
  std::ofstream ofs(filename, std::ios::trunc);
  if (not ofs)
    {
      std::cerr << "Hart::saveSnapshot failed - cannot open " << filename << " for write\n";
      return false;
    }

  // write Program Order and Program Counter
  ofs << "po " << std::dec << getInstructionCount() << "\n";
  ofs << "pc 0x" << std::hex << peekPc() << "\n";

  // write integer registers
  for(unsigned i = 1; i < 32; i++)
    ofs << "x " << std::dec << i << " 0x" << std::hex << peekIntReg(i) << "\n";

  // write floating point registers
  for (unsigned i = 0; i < 32; i++)
    {
      uint64_t val = 0;
      peekFpReg(i, val);
      ofs << "f " << std::dec << i << " 0x" << std::hex << val << "\n";
    }

  // write control & status registers
  for (unsigned i = unsigned(CsrNumber::MIN_CSR_); i <= unsigned(CsrNumber::MAX_CSR_); i++)
    {
      URV val = 0;
      if (not peekCsr(CsrNumber(i), val))
        continue;
      ofs << "c 0x" << std::hex << i << " 0x" << val << "\n";
    }

  ofs.close();
  return true;
}


/// Read an integer value fromt the given stream. Return true on
/// success and false on failure. Use strtoull to do the string to
/// integer conversion so that numbers starting with 0 or 0x are
/// interpreted as octal or hexadecimal.
static
bool
loadSnapshotValue(std::istream& stream, uint64_t& val)
{
  std::string str;
  if (not (stream >> str))
    return false;
  char* extra = nullptr;
  val = strtoull(str.c_str(), &extra, 0);
  if (extra and *extra)
    return false;
  return true;
}


/// Read from the given stream a register number and a register value.
/// Return true on success and false on failure. Use strtoull to do
/// the string to integer conversions so that numbers starting with 0
/// or 0x are interpreted as octal or hexadecimal.
static
bool
loadRegNumAndValue(std::istream& stream, unsigned& num, uint64_t& val)
{
  std::string str;
  if (not (stream >> str))
    return false;
  char* extra = nullptr;
  num = strtoull(str.c_str(), &extra, 0);
  if (extra and *extra)
    return false;
  return loadSnapshotValue(stream, val);
}


template <typename URV>
bool
Hart<URV>::loadSnapshotRegs(const std::string & filename)
{
  // open file for read, check success
  std::ifstream ifs(filename);
  if (not ifs)
    {
      std::cerr << "Hart::loadSnapshotRegs failed - cannot open " << filename << " for read\n";
      return false;
    }
  // read line by line and set registers
  std::string line;
  unsigned lineNum = 0;
  unsigned num = 0;
  uint64_t val = 0;
  bool error = false;
  while(std::getline(ifs, line))
    {
      lineNum++;
      std::istringstream iss(line);
      std::string type;
      error = true;
      // parse first part (register class)
      if (not (iss >> type))
        break; // error: parse failed

      if (type == "pc")     // PC
        {
          if (not loadSnapshotValue(iss, val))
            break; // error: parse failed
          pokePc(val);
        }
      else if (type == "po")  // Program order
        {
          if (not loadSnapshotValue(iss, val))
            break;
          setInstructionCount(val);
        }
      else if (type == "c")   // CSR
        {
          if (not loadRegNumAndValue(iss, num, val))
            break;
          if (not pokeCsr(CsrNumber(num), val))
            break; // error: poke failed
        }
      else if (type.compare("x") == 0)   // Integer register
        {  
          if (not loadRegNumAndValue(iss, num, val))
            break;
          if (not pokeIntReg(num, val))
            break; // error: poke failed
        }
      else if (type.compare("f") == 0)   // FP register
        {
          if (isRvf() or isRvd())
            {
              if (not loadRegNumAndValue(iss, num, val))
                break;
              if (not pokeFpReg(num, val))
                break; // error: poke failed
            }
        }
      else
        break;  // error: parse failed
      error = false;
    }

  if (error)
    {
      std::cerr << "Hart::loadSnapshotRegs failed to parse " << filename << ":"
                << std::dec << lineNum << "\n";
      std::cerr << "\t" << line << "\n";
    }
  ifs.close();
  return not error;
}


template class WdRiscv::Hart<uint32_t>;
template class WdRiscv::Hart<uint64_t>;