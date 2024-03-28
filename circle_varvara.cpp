#include "circle_varvara.hpp"

namespace uxn {

void CircleConsole::write_byte(u8 b) {
  if (b == '\n') flush();
  else {
    buf[buf_sz] = b;
    if (++buf_sz >= sizeof(buf) - 1) flush();
  }
}

void CircleConsole::flush() {
  if (buf_sz > 0) {
    buf[buf_sz] = '\0';
    logger.Write("Console", LogNotice, buf);
    buf_sz = 0;
  }
}

static inline bool is_end(char c) {
  return c == '/' || c == '\\' || c == '\0';
}

const char* CircleFilesystem::absolute_filename(const char* f) {
  if (f[0] == '\0') return "/";
  absolute_buffer[0] = '/';
  u8 depth = 0;
  size_t in_c = 0, out_c = 1, history[32] = {1};
  while (out_c < UXN_PATH_MAX - 1) {
    switch (f[in_c]) {
      case '/':
      case '\\':
        in_c++;
      after_slash:
        switch (f[in_c]) {
          case '/':
          case '\\':
          case '\0':
            // ignore double and trailing slashes
            break;
          case '.':
            switch (f[++in_c]) {
              case '/':
              case '\\':
              case '\0':
                // . = no-op
                break;
              case '.':
                if (is_end(f[++in_c])) {
                  // .. = go back one
                  if (depth > 0) out_c = history[--depth];
                  break;
                }
                absolute_buffer[out_c++] = '.';
                // fall through
              default:
                if (out_c < UXN_PATH_MAX - 1) absolute_buffer[out_c++] = '.';
                goto read_segment;
            }
            break;
          default:
            if (out_c > 1) absolute_buffer[out_c++] = '/';
            goto read_segment;
        }
        break;
      case '\0':
        goto done;
      case '.':
        if (out_c <= 1) goto after_slash;
        // fall through
      default:
      read_segment:
        history[depth++] = out_c - 1;
        while (!is_end(f[in_c]) && out_c < UXN_PATH_MAX - 1) {
          absolute_buffer[out_c++] = f[in_c++];
        }
    }
  }
done:
  absolute_buffer[out_c ? out_c : 1] = '\0';
  return absolute_buffer;
}

const u8* CircleFilesystem::load(const char* filename, size_t& out_size) {
  const char* path = absolute_filename(filename);
  logger.Write("File", LogNotice, "Loading ROM %s at path %s", filename, path);
  FIL fil;
  if (f_open(&fil, path, FA_READ) != FR_OK) {
    logger.Write("File", LogError, "ROM %s does not exist or cannot be opened", filename);
    return nullptr;
  }
  unsigned sz = f_size(&fil), bytes_read;
  if (!loaded_rom || loaded_rom_capacity < sz) {
    if (loaded_rom) delete[] loaded_rom;
    loaded_rom = new u8[loaded_rom_capacity = sz];
  }
  if (f_read(&fil, loaded_rom, sz, &bytes_read) != FR_OK || bytes_read < sz) {
    logger.Write("File", LogError, "ROM %s could not be read", filename);
    f_close(&fil);
    return nullptr;
  }
  f_close(&fil);
  out_size = sz;
  logger.Write("File", LogNotice, "Successfully loaded %d bytes", sz);
  return loaded_rom;
}

void CircleFilesystem::close() {
  switch (open_state) {
  case OpenState::ReadFile:
  case OpenState::Write:
  case OpenState::Append:
    if (f_close(&open_file.fil) != FR_OK) {
      logger.Write("File", LogError, "Open file %s could not be closed", open_filename);
    }
    break;
  case OpenState::ReadDir:
    if (f_closedir(&open_file.dir) != FR_OK) {
      logger.Write("File", LogError, "Open directory %s could not be closed", open_filename);
    }
    break;
  case OpenState::None:; // do nothing
  }
  open_state = OpenState::None;
}

static inline void filinfo_to_stat(FILINFO& filinfo, Stat& stat) {
  stat.name = filinfo.fname;
  if (filinfo.fattrib & AM_DIR) {
    stat.type = StatType::Directory;
  } else if (filinfo.fsize > 0xffff) {
    stat.type = StatType::LargeFile;
  } else {
    stat.type = StatType::File;
    stat.size = filinfo.fsize;
  }
}

Stat CircleFilesystem::stat() {
  close();
  if (f_stat(absolute_filename(open_filename), &last_filinfo) != FR_OK) {
    return Stat{ .type = StatType::Unavailable, .size = 0, .name = open_filename };
  }
  Stat s;
  filinfo_to_stat(last_filinfo, s);
  return s;
}

bool CircleFilesystem::list_dir(Stat& out) {
  if (open_state != OpenState::ReadDir) {
    close();
    auto path = absolute_filename(open_filename);
    if (f_opendir(&open_file.dir, path) != FR_OK) return false;
    open_state = OpenState::ReadDir;
  }
  if (f_readdir(&open_file.dir, &last_filinfo) != FR_OK) return false;
  filinfo_to_stat(last_filinfo, out);
  return true;
}

u16 CircleFilesystem::read(MutableSlice dest) {
  if (open_state != OpenState::ReadFile) {
    close();
    auto path = absolute_filename(open_filename);
    if (f_open(&open_file.fil, path, FA_READ) != FR_OK) return 0;
    open_state = OpenState::ReadFile;
  }
  unsigned bytes_read;
  if (f_read(&open_file.fil, dest.data, dest.size, &bytes_read) != FR_OK) return 0;
  return bytes_read;
}

u16 CircleFilesystem::write(Slice src, u8 append) {
  OpenState st = append ? OpenState::Append : OpenState::Write;
  if (open_state != st) {
    close();
    auto path = absolute_filename(open_filename);
    if (f_open(&open_file.fil, path, FA_WRITE | FA_CREATE_NEW | (append ? 0 : FA_CREATE_ALWAYS)) != FR_OK) return 0;
    if (append && f_lseek(&open_file.fil, f_size(&open_file.fil)) != FR_OK) return 0;
    open_state = st;
  }
  unsigned bytes_written;
  if (f_write(&open_file.fil, src.data, src.size, &bytes_written) != FR_OK) return 0;
  return bytes_written;
}

u16 CircleFilesystem::remove() {
  close();
  const char* path = absolute_filename(open_filename);
  if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) return 0;
  return f_unlink(absolute_filename(open_filename)) == FR_OK ? 1 : 0;
}

u8 CircleDatetime::datetime_byte(u8 port) {
  CTime time;
  time.Set(timer.GetLocalTime());
  switch (port) {
    case 0x0: return time.GetYear() >> 8;
    case 0x1: return time.GetYear();
    case 0x2: return time.GetMonth();
    case 0x3: return time.GetMonthDay();
    case 0x4: return time.GetHours();
    case 0x5: return time.GetMinutes();
    case 0x6: return time.GetSeconds();
    case 0x7: return time.GetWeekDay();
    case 0x8: return 0; // TODO: day of year
    case 0x9: return 0;
    case 0xa: return 0; // TODO: DST
    default: return 0;
  }
}

void CircleVarvara::game_pad_input(const TGamePadState* state) {
  //if (state->buttons & TGamePadButton::GamePadButtonGuide) {
  //  reset(false);
  //  return;
  //}

  // just skip input methods completely,
  // and write directly to the device buffer
  dev[0x82] =
    (state->buttons & (TGamePadButton::GamePadButtonA | TGamePadButton::GamePadButtonX) ? 0x1 : 0) |
    (state->buttons & (TGamePadButton::GamePadButtonB | TGamePadButton::GamePadButtonY) ? 0x2 : 0) |
    (state->buttons & (TGamePadButton::GamePadButtonSelect | TGamePadButton::GamePadButtonMinus) ? 0x4 : 0) |
    (state->buttons & (TGamePadButton::GamePadButtonStart | TGamePadButton::GamePadButtonPlus) ? 0x8 : 0) |
    (state->buttons & TGamePadButton::GamePadButtonUp ? 0x10 : 0) |
    (state->buttons & TGamePadButton::GamePadButtonDown ? 0x20 : 0) |
    (state->buttons & TGamePadButton::GamePadButtonLeft ? 0x40 : 0) |
    (state->buttons & TGamePadButton::GamePadButtonRight ? 0x80 : 0);
  call_vec(0x80);
  console.flush();
}

void CircleVarvara::run() {
  eval(PAGE_PROGRAM);
  screen.repaint();
  console.flush();
  u64 current_ticks = timer.GetClockTicks64();
  while (true) {
    screen.frame();
    console.flush();

    // Sync at 60 Hz.
    if ((timer.GetClockTicks64() - current_ticks) < 16666) {
      timer.usDelay(16666 - (timer.GetClockTicks64() - current_ticks));
    }
    current_ticks = timer.GetClockTicks64();
  }
}

}
