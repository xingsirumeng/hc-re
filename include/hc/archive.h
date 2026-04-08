#pragma once
#include <archive.h>

#include <filesystem>
#include <locale>
#include <span>
#include <string>
#include <vector>

namespace hc::archive {

namespace fs = std::filesystem;

class ArchiveEntry {
  public:
    /// @brief  Copys staticstics from disk_path, as ar_path.
    ArchiveEntry(fs::path const &disk_path, std::u8string const &ar_path);

    ArchiveEntry(ArchiveEntry const &) = delete;
    ArchiveEntry(ArchiveEntry &&other) noexcept;
    ArchiveEntry &operator=(ArchiveEntry const &) = delete;
    ArchiveEntry &operator=(ArchiveEntry &&other) noexcept;

    ~ArchiveEntry();

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::string_view pathname() const;

    [[nodiscard]] struct archive_entry *get();
    [[nodiscard]] struct archive_entry const *get() const;

  private:
    struct archive_entry *entry_;
};

// Output Archive
class OArchive {
  public:
    OArchive(OArchive const &) = delete;
    OArchive(OArchive &&) = delete;
    OArchive &operator=(OArchive const &) = delete;
    OArchive &operator=(OArchive &&) = delete;

    OArchive() : archive_{archive_write_new()}
    {
        if (archive_ == nullptr) {
            throw std::runtime_error{"Failed to create archive"};
        }
    }

    ~OArchive()
    {
        close();
        archive_write_free(archive_);
    }

    void open(fs::path const &file)
    {
        close();
        if (archive_write_open_filename(archive_, file.c_str()) != ARCHIVE_OK) {
            throw std::runtime_error{archive_error_string(archive_)};
        }
        file_opened_ = true;
    }
    void close()
    {
        if (file_opened_) {
            archive_write_close(archive_);
            file_opened_ = false;
        }
    }

    void set_format(int format)
    {
        if (archive_write_set_format(archive_, format) != ARCHIVE_OK) {
            throw std::runtime_error{archive_error_string(archive_)};
        }
    }

    /// @param filter  Sane options are ARCHIVE_FILTER_XXXs.
    void add_filter(int filter)
    {
        if (archive_write_add_filter(archive_, filter) != ARCHIVE_OK) {
            throw std::runtime_error{archive_error_string(archive_)};
        }
    }

    struct archive *get()
    {
        return archive_;
    }

  private:
    struct archive *archive_;
    bool file_opened_{};
};

class ArchiveWriter {
  public:
    /// @param format   Sane options are ARCHIVE_FORMAT_XXXs.
    /// @param filters  Sane options are ARCHIVE_FILTER_XXXs.
    template <std::ranges::range Filters = std::vector<int>>
    explicit ArchiveWriter(fs::path const &out,
                           int format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED,
                           Filters const &filters = {});

    ArchiveWriter(ArchiveWriter const &) = delete;
    ArchiveWriter(ArchiveWriter &&) = delete;
    ArchiveWriter &operator=(ArchiveWriter const &) = delete;
    ArchiveWriter &operator=(ArchiveWriter &&) = delete;

    ~ArchiveWriter() = default;

    /// @brief  Supports both directory and file.
    /// TODO: Currently lost all meta information.
    void add_path(fs::path const &disk_path, std::u8string const &ar_path);

    /// @brief  Sets internal locale to use. Default value is C.
    std::locale imbue(std::locale const &l);

  private:
    void throw_on_error(int res);
    void write_file(fs::path const &disk_path, std::u8string const &ar_path);
    void write_directory_recursive(fs::path disk_path,
                                   std::u8string const &ar_path);
    void write_symlink(fs::path const &disk_path, std::u8string const &ar_path);

    OArchive oa_;
    std::locale locale_;
};

// Create .tar.zst archives using libarchive. Function throws on failure.
void create_tar_zst(fs::path const &out_path, std::span<fs::path> const &paths);

// This may be not needed since we only output tar.zst but not input them.
// bool extract_tar_zst(fs::path const &archive_path, fs::path const &dest_dir,
//                      std::string &err);

} // namespace hc::archive
