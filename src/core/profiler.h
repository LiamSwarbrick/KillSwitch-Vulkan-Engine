#ifndef CORE_PROFILE_H
#define CORE_PROFILE_H

// Lightweight CPU profiler with CSV export.
// Header-only: define CORE_PROFILE_IMPLEMENTATION in exactly one .cpp.
//
// Usage:
//   Profile_BeginCapture("profile/wave1.csv", /*wave*/ 1, /*max_frames*/ 600);
//   Per frame:
//     Profile_FrameBegin(currentWave);
//     { ProfileScope s("Physics");   physics_step(); }
//     { ProfileScope s("Animation"); animation_step(); }
//     Profile_FrameEnd();
//   After max_frames (or via Profile_EndCapture()) the CSV is flushed.
//
//  - All times are milliseconds (std::chrono::steady_clock).
//  - Section names must be string literals (stored as const char*).
//  - Multiple scopes with the same name in one frame accumulate.

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

struct ProfileSection { const char* name; double ms; };

struct ProfileFrameRow
{
    int                 frame_index;
    int                 wave;
    double              total_ms;
    std::vector<double> section_ms; // aligned to ProfileState::column_order
};

struct ProfileState
{
    bool                                  capturing = false;
    int                                   wave_tag = 0;
    int                                   frame_index = 0;
    int                                   max_frames = 0;
    std::vector<ProfileSection>           current_frame;
    std::vector<const char*>              column_order;
    std::vector<ProfileFrameRow>          rows;
    std::string                           out_path;
    std::chrono::steady_clock::time_point frame_start;
};

ProfileState& Profile_Get();

void Profile_BeginCapture(const char* out_path, int wave_tag, int max_frames);
void Profile_EndCapture();
void Profile_FrameBegin(int wave_tag);
void Profile_FrameEnd();
void Profile_AddSection(const char* name, double ms);
bool Profile_IsCapturing();

// Headless profile run flag. Set once at startup in main.cpp when running
// with --profile-wave; consumed elsewhere to tweak gameplay parameters for
// deterministic / lightweight measurement (e.g. shrink zombie alert range so
// AI work is dominated by simple idle paths rather than chase logic).
void Profile_SetProfileModeActive(bool active);
bool Profile_IsProfileModeActive();

class ProfileScope
{
public:
    explicit ProfileScope(const char* name)
        : m_name(name), m_start(std::chrono::steady_clock::now()) {}
    ~ProfileScope()
    {
        if (!Profile_IsCapturing()) return;
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
        Profile_AddSection(m_name, ms);
    }
private:
    const char* m_name;
    std::chrono::steady_clock::time_point m_start;
};

#define PROFILE_SCOPE_CAT2(a,b) a##b
#define PROFILE_SCOPE_CAT(a,b)  PROFILE_SCOPE_CAT2(a,b)
#define PROFILE_SCOPE(name)     ProfileScope PROFILE_SCOPE_CAT(_prof_, __LINE__)(name)

// Inline helpers for callers that need to time arbitrary code spans without RAII scoping.
// Example:
//   auto t0 = Profile_Now();
//   ... work ...
//   Profile_AddSection("Renderer_RecordCmds", Profile_ElapsedMs(t0));
inline std::chrono::steady_clock::time_point Profile_Now()
{
    return std::chrono::steady_clock::now();
}
inline double Profile_ElapsedMs(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

#ifdef CORE_PROFILE_IMPLEMENTATION

ProfileState& Profile_Get()
{
    static ProfileState s;
    return s;
}

bool Profile_IsCapturing() { return Profile_Get().capturing; }

static bool& Profile_ModeActiveFlag()
{
    static bool s_active = false;
    return s_active;
}
void Profile_SetProfileModeActive(bool active) { Profile_ModeActiveFlag() = active; }
bool Profile_IsProfileModeActive()             { return Profile_ModeActiveFlag(); }

void Profile_BeginCapture(const char* out_path, int wave_tag, int max_frames)
{
    ProfileState& s = Profile_Get();
    if (s.capturing) Profile_EndCapture();
    s.capturing   = true;
    s.wave_tag    = wave_tag;
    s.frame_index = 0;
    s.max_frames  = max_frames;
    s.current_frame.clear();
    s.column_order.clear();
    s.rows.clear();
    s.out_path    = out_path ? out_path : "profile/capture.csv";
    std::fprintf(stdout, "[Profile] Begin capture -> %s (wave=%d, max_frames=%d)\n",
                 s.out_path.c_str(), wave_tag, max_frames);
}

static int Profile_FindOrAddColumn(const char* name)
{
    ProfileState& s = Profile_Get();
    for (size_t i = 0; i < s.column_order.size(); ++i)
        if (std::strcmp(s.column_order[i], name) == 0) return (int)i;
    s.column_order.push_back(name);
    return (int)s.column_order.size() - 1;
}

void Profile_FrameBegin(int wave_tag)
{
    ProfileState& s = Profile_Get();
    if (!s.capturing) return;
    s.wave_tag = wave_tag;
    s.current_frame.clear();
    s.frame_start = std::chrono::steady_clock::now();
}

void Profile_AddSection(const char* name, double ms)
{
    ProfileState& s = Profile_Get();
    if (!s.capturing) return;
    for (auto& sec : s.current_frame)
        if (std::strcmp(sec.name, name) == 0) { sec.ms += ms; return; }
    s.current_frame.push_back({ name, ms });
}

void Profile_FrameEnd()
{
    ProfileState& s = Profile_Get();
    if (!s.capturing) return;
    auto end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - s.frame_start).count();

    ProfileFrameRow row;
    row.frame_index = s.frame_index;
    row.wave        = s.wave_tag;
    row.total_ms    = total_ms;
    row.section_ms.assign(s.column_order.size(), 0.0);
    for (const auto& sec : s.current_frame)
    {
        int col = Profile_FindOrAddColumn(sec.name);
        if ((size_t)col >= row.section_ms.size()) row.section_ms.resize(col + 1, 0.0);
        row.section_ms[col] = sec.ms;
    }
    s.rows.push_back(std::move(row));
    s.frame_index++;

    if (s.max_frames > 0 && s.frame_index >= s.max_frames)
        Profile_EndCapture();
}

void Profile_EndCapture()
{
    ProfileState& s = Profile_Get();
    if (!s.capturing) return;
    s.capturing = false;

    FILE* f = std::fopen(s.out_path.c_str(), "wb");
    if (!f)
    {
        std::fprintf(stderr, "[Profile] Failed to open %s for write\n", s.out_path.c_str());
        return;
    }

    std::fprintf(f, "frame,wave,total_ms");
    for (auto* col : s.column_order)
        std::fprintf(f, ",%s", col);
    std::fprintf(f, "\n");

    for (const auto& r : s.rows)
    {
        std::fprintf(f, "%d,%d,%.6f", r.frame_index, r.wave, r.total_ms);
        for (size_t c = 0; c < s.column_order.size(); ++c)
        {
            double v = (c < r.section_ms.size()) ? r.section_ms[c] : 0.0;
            std::fprintf(f, ",%.6f", v);
        }
        std::fprintf(f, "\n");
    }
    std::fclose(f);
    std::fprintf(stdout, "[Profile] Wrote %zu frames to %s\n", s.rows.size(), s.out_path.c_str());

    s.rows.clear();
    s.column_order.clear();
    s.current_frame.clear();
}

#endif // CORE_PROFILE_IMPLEMENTATION

#endif // CORE_PROFILE_H
