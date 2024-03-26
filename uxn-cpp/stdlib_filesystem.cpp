#include "stdlib_filesystem.hpp"
#include <iostream>
#include <sys/stat.h>

using std::cerr, std::filesystem::directory_iterator, std::endl,
    std::error_code, std::get_if, std::holds_alternative, std::ifstream,
    std::ios, std::monostate, std::ofstream, std::filesystem::weakly_canonical;

namespace uxn {

  bool StdlibFilesystem::init() {
    error_code ec;
    root_dir = weakly_canonical(original_root_dir, ec);
    if (ec) {
      cerr << "Invalid root directory " << original_root_dir << ": " << ec << endl;
      return false;
    }
    return true;
  }

  const u8 *StdlibFilesystem::load(const char *filename, size_t &out_size) {
    error_code ec;
    auto path = weakly_canonical(root_dir / filename, ec);
    if (ec) {
      cerr << "Invalid ROM path " << filename << ": " << ec << endl;
      return nullptr;
    }
    if (!is_in_root_dir(path)) {
      cerr << "ROM path " << path << " is not in sandbox directory " << root_dir << endl;
      return nullptr;
    }
    ifstream file(path, ios::binary | ios::ate);
    auto sz = file.tellg();
    if (!file.is_open()) {
      cerr << "ROM " << filename << " does not exist or cannot be opened" << endl;
      return nullptr;
    }
    loaded_rom.resize(sz);
    file.seekg(0, ios::beg);
    if (!file.read((char *)loaded_rom.data(), sz)) {
      cerr << "ROM " << filename << " could not be read" << endl;
      return nullptr;
    }
    out_size = sz;
    return loaded_rom.data();
  }

  Stat StdlibFilesystem::stat() {
    error_code ec;
    const char *filename = open_filename;
    auto path = weakly_canonical(root_dir / filename, ec);
    if (ec || !is_in_root_dir(path)) {
      return { .type = StatType::Unavailable, .name = filename };
    }
    struct stat st;
    int err = ::stat(path.c_str(), &st);
    if (err) {
      return { .type = StatType::Unavailable, .name = filename };
    } if (st.st_mode & S_IFDIR) {
      return { .type = StatType::Directory, .name = filename };
    } else if (st.st_mode & S_IFREG) {
      if (st.st_size > 0xffff) {
        return { .type = StatType::LargeFile, .size = 0xffff, .name = filename };
      } else {
        return { .type = StatType::File, .size = (u16)st.st_size, .name = filename};
      }
    }
    return {.type = StatType::Unavailable, .name = filename};
  }

  bool StdlibFilesystem::list_dir(Stat & out) {
    error_code ec;
    if (!holds_alternative<directory_iterator>(open_file)) {
      auto path = weakly_canonical(root_dir / open_filename, ec);
      if (ec || !is_in_root_dir(path)) return false;
      open_file = directory_iterator(path);
    }
    if (auto *dir = get_if<directory_iterator>(&open_file)) {
      if (*dir == directory_iterator()) return false;
      auto entry = **dir;
      last_dir_entry_name = entry.path().lexically_relative(root_dir);
      out.name = last_dir_entry_name.c_str();
      if (entry.is_directory()) {
        out.type = StatType::Directory;
      } else {
        auto size = entry.file_size(ec);
        if (ec) out.type = StatType::Unavailable;
        else if (size > 0xffff) out.type = StatType::LargeFile;
        else {
          out.type = StatType::File;
          out.size = size;
        }
      }
      (*dir)++;
      return true;
    }
    return false;
  }

  u16 StdlibFilesystem::read(MutableSlice dest) {
    error_code ec;
    if (!holds_alternative<ifstream>(open_file)) {
      auto path = weakly_canonical(root_dir / open_filename, ec);
      if (ec || !is_in_root_dir(path)) return 0;
      open_file = ifstream(path.c_str());
    }
    if (auto *f = get_if<ifstream>(&open_file)) {
      if (!f->is_open()) return 0;
      f->read((char *)dest.data, dest.size);
      return f->gcount();
    }
    return 0;
  }

  u16 StdlibFilesystem::write(Slice src, u8 append) {
    error_code ec;
    if (!holds_alternative<ofstream>(open_file)) {
      auto path = weakly_canonical(root_dir / open_filename, ec);
      if (ec || !is_in_root_dir(path)) return 0;
      if (append) open_file = ofstream(path.c_str(), ios::app);
      else open_file = ofstream(path.c_str());
    }
    if (auto *f = get_if<ofstream>(&open_file)) {
      if (!f->is_open()) return 0;
      auto before = f->tellp();
      f->write((const char *)src.data, src.size);
      return static_cast<u16>(f->tellp() - before);
    }
    return 0;
  }

  u16 StdlibFilesystem::remove() {
    open_file = monostate();
    error_code ec;
    auto path = weakly_canonical(root_dir / open_filename, ec);
    if (ec || !is_in_root_dir(path)) return 0;
    return std::filesystem::remove(path, ec) ? 1 : 0;
  }
}
