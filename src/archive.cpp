#include <hc/archive.h>

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

namespace hc::archive {

// create_tar_zst: minimal implementationA
// - supports only regular files, directories, and symlinks
// - stores each input file under its filename (basename) in the archive
// - does not attempt to preserve ownership/complex metadata
void create_tar_zst(fs::path const &out_path, std::span<fs::path> const &paths)
{
    auto doesnt_exist = [](auto const &p) { return !fs::exists(p); };
    if (std::ranges::any_of(paths, doesnt_exist)) {
        throw std::runtime_error{"File doesn't exist"};
    }

    ArchiveWriter aw(out_path, ARCHIVE_FORMAT_TAR_PAX_RESTRICTED,
                     {ARCHIVE_FILTER_ZSTD});
    aw.imbue(std::locale("en_US.UTF-8"));
    for (auto const &p : paths) {
        // Add to root
        aw.add_path(p, p.filename().u8string());
    }
}

// extract_tar_zst: minimal implementation
// - only extracts regular file entries
// - writes files under dest_dir, preserving entry's pathname but sanitized
// - does not restore ownership or complex metadata
// bool extract_tar_zst(fs::path const &archive_path, fs::path const &dest_dir,
//                      std::string &err)
// {
//     struct archive *a = archive_read_new();
//     if (!a) {
//         err = "archive_read_new failed";
//         return false;
//     }
//
//     // allow zstd filter and tar format
//     archive_read_support_filter_by_name(a, "zstd");
//     archive_read_support_format_tar(a);
//
//     std::string arc_s = path_to_u8string(archive_path);
//     if (archive_read_open_filename(a, arc_s.c_str(), 10240) != ARCHIVE_OK) {
//         err = archive_error_string(a);
//         archive_read_free(a);
//         return false;
//     }
//
//     // ensure dest_dir exists
//     std::error_code ec;
//     if (!fs::exists(dest_dir, ec)) {
//         if (!fs::create_directories(dest_dir, ec)) {
//             err = "failed to create dest dir: " + ec.message();
//             archive_read_close(a);
//             archive_read_free(a);
//             return false;
//         }
//     }
//
//     struct archive_entry *entry = nullptr;
//     int r = ARCHIVE_OK;
//     while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
//         char const *pathname = archive_entry_pathname(entry);
//         if (!pathname) {
//             archive_read_data_skip(a);
//             continue;
//         }
//
//         // sanitize entry name
//         std::string sanitized = sanitize_entry_name(pathname, err);
//         if (sanitized.empty()) {
//             // sanitize_entry_name sets err
//             archive_read_close(a);
//             archive_read_free(a);
//             return false;
//         }
//
//         // we only accept regular file entries
//         int filetype = archive_entry_filetype(entry);
//         if (filetype != AE_IFREG) {
//             // skip non-regular entries
//             archive_read_data_skip(a);
//             continue;
//         }
//
//         fs::path outp = dest_dir / fs::path(sanitized);
//         // create parent directories
//         std::error_code ec2;
//         if (outp.has_parent_path())
//             fs::create_directories(outp.parent_path(), ec2);
//
//         std::ofstream ofs(path_to_u8string(outp),
//                           std::ios::binary | std::ios::trunc);
//         if (!ofs) {
//             err = "failed to create output file: " + path_to_u8string(outp);
//             archive_read_close(a);
//             archive_read_free(a);
//             return false;
//         }
//
//         // read data and write to file
//         void const *buff;
//         size_t size;
//         la_int64_t offset;
//         while ((r = archive_read_data_block(a, &buff, &size, &offset)) ==
//                ARCHIVE_OK) {
//             ofs.write(reinterpret_cast<char const *>(buff),
//                       static_cast<std::streamsize>(size));
//             if (!ofs) {
//                 err = "write failed for output file: " +
//                 path_to_u8string(outp); archive_read_close(a);
//                 archive_read_free(a);
//                 return false;
//             }
//         }
//         if (r != ARCHIVE_EOF && r != ARCHIVE_OK) {
//             err = archive_error_string(a);
//             archive_read_close(a);
//             archive_read_free(a);
//             return false;
//         }
//         // finished entry; continue to next
//     }
//
//     if (r != ARCHIVE_EOF) {
//         err = archive_error_string(a);
//         archive_read_close(a);
//         archive_read_free(a);
//         return false;
//     }
//
//     archive_read_close(a);
//     archive_read_free(a);
//     return true;
// }

template <std::ranges::range Filters>
ArchiveWriter::ArchiveWriter(fs::path const &out, int format,
                             Filters const &filters)
{
    // Invocation order here is important. Don't change unless you know what
    // you're doing.
    oa_.set_format(format);
    for (auto const &filter : filters) {
        oa_.add_filter(filter);
    }
    oa_.open(out);
}

void ArchiveWriter::add_path(fs::path const &disk_path,
                             std::u8string const &ar_path)
{
    // We should use C locale here because libarchive reads only from it.
    auto *old = std::setlocale(LC_CTYPE, locale_.name().c_str());

    auto ftype = fs::symlink_status(disk_path).type();
    switch (ftype) {
    case std::filesystem::file_type::regular:
        write_file(disk_path, ar_path);
        break;
    case std::filesystem::file_type::directory:
        write_directory_recursive(disk_path, ar_path);
        break;
    case std::filesystem::file_type::symlink:
        write_symlink(disk_path, ar_path);
        break;
    case std::filesystem::file_type::not_found:
        throw std::runtime_error{"No such file or directory"};
    default:
        throw std::runtime_error{"Unsupported file type"};
    }

    // Restore original locale as we just changed it.
    std::setlocale(LC_CTYPE, old);
}

void ArchiveWriter::write_file(fs::path const &disk_path,
                               std::u8string const &ar_path)
{
    ArchiveEntry entry(disk_path, ar_path);
    spdlog::debug(R"(Writing entry "{}" as "{}")", disk_path.string(),
                  entry.pathname());
    throw_on_error(archive_write_header(oa_.get(), entry.get()));
    if (entry.size() > 0) {
        std::ifstream ifs(disk_path, std::ios::binary);
        if (!ifs.is_open()) {
            throw std::runtime_error{
                std::format("Failed to open '{}'", disk_path.string())};
        }
        constexpr auto buf_sz = 4UZ << 10;
        std::array<char, buf_sz> buf{};
        while (true) {
            ifs.read(buf.data(), buf_sz);
            auto const n = static_cast<std::size_t>(ifs.gcount());
            if (n == 0) {
                break;
            }
            auto const written = static_cast<std::size_t>(
                archive_write_data(oa_.get(), buf.data(), n));
            if (written != n) {
                throw std::runtime_error{archive_error_string(oa_.get())};
            }
        }
    }
}

void ArchiveWriter::write_directory_recursive(fs::path disk_path,
                                              std::u8string const &ar_path)
{
    disk_path = disk_path.lexically_normal();

    assert(fs::symlink_status(disk_path).type() == fs::file_type::directory);
    // Writes header for the root
    {
        ArchiveEntry entry(disk_path, ar_path);
        spdlog::debug(R"(Writing entry "{}" as "{}")", disk_path.string(),
                      entry.pathname());
        throw_on_error(archive_write_header(oa_.get(), entry.get()));
    }

    auto const dir = fs::recursive_directory_iterator{disk_path};
    for (auto const &ent : dir) {
        auto const &p = ent.path();
        auto const target_relative =
            ar_path + u8"/" + fs::relative(p, disk_path).u8string();
        if (ent.is_regular_file()) {
            write_file(p, target_relative);
        }
        else if (ent.is_directory()) {
            ArchiveEntry entry(p, target_relative);
            spdlog::debug(R"(Writing entry "{}" as "{}")", p.string(),
                          entry.pathname());
            throw_on_error(archive_write_header(oa_.get(), entry.get()));
        }
        else if (ent.is_symlink()) {
            write_symlink(p, target_relative);
        }
    }
}

void ArchiveWriter::write_symlink(fs::path const &disk_path,
                                  std::u8string const &ar_path)
{
    ArchiveEntry entry(disk_path, ar_path);
    spdlog::debug(R"(Writing entry "{}" as "{}")", disk_path.string(),
                  entry.pathname());
    throw_on_error(archive_write_header(oa_.get(), entry.get()));
}

// version without utf8 doesn't corrupt. ??..
ArchiveEntry::ArchiveEntry(fs::path const &disk_path,
                           std::u8string const &ar_path)
    : entry_{}
{
    if (!fs::exists(disk_path)) {
        throw std::runtime_error{"No such file or directory"};
    }

    auto const fstatus = fs::symlink_status(disk_path);
    auto const ftype = fstatus.type();
    if (ftype != fs::file_type::regular && ftype != fs::file_type::directory &&
        ftype != fs::file_type::symlink) {
        throw std::runtime_error(
            "Only regular files, directories and symlink are supported");
    }

    entry_ = archive_entry_new();
    if (entry_ == nullptr) {
        throw std::runtime_error{"Failed to create archive entry"};
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    switch (ftype) {
    case fs::file_type::regular: {
        archive_entry_set_filetype(entry_, AE_IFREG);
        archive_entry_set_pathname(
            entry_, reinterpret_cast<char const *>(ar_path.c_str()));

        auto const fsize = fs::file_size(disk_path);
        archive_entry_set_size(entry_, static_cast<la_int64_t>(fsize));

        auto mode = static_cast<mode_t>(fstatus.permissions()) & 07777U;
        archive_entry_set_perm(entry_, mode);
        break;
    }
    case fs::file_type::directory: {
        archive_entry_set_filetype(entry_, AE_IFDIR);
        auto p = ar_path;
        if (!p.ends_with('/')) {
            p += '/';
        }
        archive_entry_set_pathname(entry_,
                                   reinterpret_cast<char const *>(p.c_str()));
        archive_entry_set_size(entry_, 0);

        auto mode = static_cast<mode_t>(fstatus.permissions()) & 07777U;
        mode |= (mode & 0444) >> 2; // (GNU tar behavior?
        archive_entry_set_perm(entry_, mode);
        break;
    }
    case fs::file_type::symlink: {
        archive_entry_set_filetype(entry_, AE_IFLNK);
        archive_entry_set_pathname(
            entry_, reinterpret_cast<char const *>(ar_path.c_str()));
        archive_entry_set_symlink(entry_,
                                  fs::read_symlink(disk_path).string().c_str());
        break;
    }
    default:
        std::unreachable();
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    auto const ftime = fs::last_write_time(disk_path);

    using namespace std::chrono;
    // 转换到 system_clock（避免 file_time_type epoch 不一致）
    auto const sctp_full = clock_cast<system_clock>(ftime);
    auto const sctp = time_point_cast<seconds>(sctp_full);
    auto const ns = duration_cast<nanoseconds>(sctp_full - sctp);
    archive_entry_set_mtime(entry_, sctp.time_since_epoch().count(),
                            ns.count());
}

ArchiveEntry::ArchiveEntry(ArchiveEntry &&other) noexcept : entry_(other.entry_)
{
    other.entry_ = nullptr;
}

ArchiveEntry &ArchiveEntry::operator=(ArchiveEntry &&other) noexcept
{
    if (this != &other) {
        if (entry_ != nullptr)
            archive_entry_free(entry_);
        entry_ = other.entry_;
        other.entry_ = nullptr;
    }
    return *this;
}

ArchiveEntry::~ArchiveEntry()
{
    if (entry_ != nullptr) {
        archive_entry_free(entry_);
    }
}

struct archive_entry *ArchiveEntry::get()
{
    return entry_;
}

struct archive_entry const *ArchiveEntry::get() const
{
    return entry_;
}

void ArchiveWriter::throw_on_error(int res)
{
    if (res != ARCHIVE_OK) {
        throw std::runtime_error{archive_error_string(oa_.get())};
    }
}

[[nodiscard]] std::size_t ArchiveEntry::size() const
{
    return static_cast<std::size_t>(archive_entry_size(entry_));
}

[[nodiscard]] std::string_view ArchiveEntry::pathname() const
{
    return archive_entry_pathname(entry_);
}

std::locale ArchiveWriter::imbue(std::locale const &l)
{
    return std::exchange(locale_, l);
}

} // namespace hc::archive
