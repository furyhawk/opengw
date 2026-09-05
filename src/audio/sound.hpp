#pragma once

#include <SDL3/SDL.h>

#include <vector>

class sound
{
  public:
    sound();
    ~sound();

    void loadTrack(const char* file, std::size_t track, float volume, bool loop = false);
    void playTrack(std::size_t track);
    void stopTrack(std::size_t track);
    void stopAllTracks();
    void stopAllTracksBut(std::size_t track);

    // Master volume for every track (0.0f .. 1.0f, default 1.0f).
    void setMasterVolume(float volume);
    float getMasterVolume() const;
    int playTrackGroup(std::size_t trackFirst, std::size_t trackLast);
    void pauseTrack(std::size_t track);
    void unpauseTrack(std::size_t track);
    void pauseAllTracks();
    void unpauseAllTracks();
    void pauseAllTracksBut(std::size_t track);
    bool isTrackPlaying(std::size_t track);

    void startSound();
    void stopSound();

    void setTrackSpeed(std::size_t track, double speed);

  private:
    static void bufferCallback(void* userdata, Uint8* stream, int len);
    static void sdl3AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

    struct TRACK
    {
        std::vector<Sint16> data;
        bool loop = false;
        bool playing = false;
        bool paused = false;
        float vol = 0.0f;
        double speed = 0.0;
        double pos = 0.0;
        Uint32 len = 0;
    };

    std::vector<TRACK> mTracks;

    std::vector<float> mLeftSamples;
    std::vector<float> mRightSamples;

    float mMasterVolume { 1.0f };

    SDL_AudioStream* stream { nullptr };
};
