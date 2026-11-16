#pragma once

#include "GameStreamClient.hpp"
#include "MoonlightSessionDecoderAndRenderProvider.hpp"
#include <atomic>
#include <deque>
#include <nanovg.h>

struct SessionStats {
    VideoDecodeStats video_decode_stats;
    VideoRenderStats video_render_stats;
};

class MoonlightSession {
  public:
    static void
    set_provider(MoonlightSessionDecoderAndRenderProvider* provider);

    MoonlightSession(const std::string& address, int app_id);
    ~MoonlightSession();

    static MoonlightSession* activeSession();

    void start(ServerCallback<bool> callback, bool is_sunshine);
    void stop(int terminate_app);
    void set_address(const std::string& address) { m_address = address; }

    enum class Status : unsigned char {
        // Connecting for the first time.
        None,
        Active,
        // The stream has ended; the view should close.
        Terminate,
        // A connection error happened on a worker thread. The main thread
        // must perform the restart (see performRestart()).
        RestartPending,
        Restarting,
    };

    // Safe to call from any thread: only records that a restart is needed.
    void scheduleRestart();
    // Must be called from the main (UI) thread.
    void performRestart();

    void draw(NVGcontext* vg, int width, int height);

    Status status() const { return m_status.load(); }
    bool is_active() const { return status() == Status::Active; }
    bool is_terminated() const { return status() == Status::Terminate; }
    bool is_reconnecting() const {
        const Status s = status();
        return s == Status::RestartPending || s == Status::Restarting;
    }

    bool connection_status_is_poor() const {
        return m_connection_status_is_poor;
    }

    bool use_hdr() const {
        return m_use_hdr;
    }

    SessionStats* session_stats() const {
        return (SessionStats*)&m_session_stats;
    }

  private:
    static void connection_stage_starting(int);
    static void connection_stage_complete(int);
    static void connection_stage_failed(int, int);
    static void connection_started();
    static void connection_terminated(int);
    static void connection_log_message(const char* format, ...);
    static void connection_rumble(unsigned short, unsigned short,
                                  unsigned short);
    static void connection_rumble_triggers(uint16_t controllerNumber,
                                           uint16_t leftTriggerMotor, uint16_t rightTriggerMotor);
    static void connection_status_update(int);
    static void connection_set_hdr_mode(bool);

    static int video_decoder_setup(int, int, int, int, void*, int);
    static void video_decoder_start();
    static void video_decoder_stop();
    static void video_decoder_cleanup();
    static int video_decoder_submit_decode_unit(PDECODE_UNIT);

    static int audio_renderer_init(int, const POPUS_MULTISTREAM_CONFIGURATION,
                                   void*, int);
    static void audio_renderer_start();
    static void audio_renderer_stop();
    static void audio_renderer_cleanup();
    static void audio_renderer_decode_and_play_sample(char*, int);

    std::string m_address;
    int m_app_id;
    bool m_is_sunshine = false;
    STREAM_CONFIGURATION m_config;
    CONNECTION_LISTENER_CALLBACKS m_connection_callbacks;
    DECODER_RENDERER_CALLBACKS m_video_callbacks;
    AUDIO_RENDERER_CALLBACKS m_audio_callbacks;

    IFFmpegVideoDecoder* m_video_decoder = nullptr;
    IVideoRenderer* m_video_renderer = nullptr;
    IAudioRenderer* m_audio_renderer = nullptr;

    // Written by moonlight-common-c / decoder threads, read by the UI thread.
    std::atomic<Status> m_status{Status::None};
    // Set when the user asked to stop (not a network error).
    std::atomic<bool> m_stop_requested{false};
    bool m_connection_status_is_poor = false;
    bool m_use_hdr = false;

    // Main thread only: start times (LiGetMillis) of recent reconnects, used
    // to give up when the connection keeps dropping.
    std::deque<uint64_t> m_recent_restarts;

    SessionStats m_session_stats = {};
    uint64_t m_last_stats_update_ms = 0;
};
