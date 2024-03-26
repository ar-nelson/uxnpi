#pragma once
#include "varvara.hpp"
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <variant>

namespace uxn {

class StdlibFilesystem : public Filesystem {
private:
  std::filesystem::path original_root_dir, root_dir;
  std::variant<
    std::monostate,
    std::ifstream,
    std::filesystem::directory_iterator,
    std::ofstream
  > open_file;
  std::string last_dir_entry_name;
  std::vector<u8> loaded_rom;

  bool is_in_root_dir(std::filesystem::path p) {
    auto [root_end, nothing] = std::mismatch(root_dir.begin(), root_dir.end(), p.begin());
    return root_end == root_dir.end();
  }

public:
  StdlibFilesystem(Uxn& uxn, std::filesystem::path root_dir) : Filesystem(uxn), original_root_dir(root_dir) {}
  virtual ~StdlibFilesystem() {}

  virtual bool init();
  const u8* load(const char* filename, size_t& out_size) final;

protected:
  void open() final { open_file = std::monostate(); }
  Stat stat() final;
  bool list_dir(Stat& out) final;
  u16 read(MutableSlice dest) final;
  u16 write(Slice src, u8 append) final;
  u16 remove() final;
};

}
