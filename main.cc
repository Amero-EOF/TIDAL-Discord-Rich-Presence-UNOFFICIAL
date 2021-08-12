/**
 * @file    main.cc
 * @authors Stavros Avramidis
 */


/* C++ libs */
#include <atomic>
#include <cctype>
#include <chrono>
#include <locale>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <deque>
/* C libs */
#include <cstdio>
/* Qt */
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

/* local libs*/
#include "discord_game_sdk.h"
#include "httplib.hh"
#include "json.hh"

#ifdef WIN32

#include "windows_api_hook.hh"

#elif defined(__APPLE__) or defined(__MACH__)

#include "osx_api_hook.hh"

#else
#error "Not supported target"
#endif
#define CURRENT_TIME std::time(nullptr)
#define HIFI_ASSET "hifi"
#define TIME_CHARACTERS 12
#define MAX_PLAYER_CHARS (32 - TIME_CHARACTERS)
#define MAX_PLAYER_CHARS_MINUS_ONE (MAX_PLAYER_CHARS - 1)
#define AUTOSCROLL_CAP 24
static long long APPLICATION_ID = 584458858731405315;
int64_t currentTime = 0;


std::atomic<bool> isPresenceActive;

static char *countryCode = nullptr;



static std::string currentStatus;
static std::mutex currentSongMutex;


struct Song {
    enum AudioQualityEnum { master, hifi, normal };
    std::string title;
    std::string artist;
    std::string album;
    std::string url;
    char id[10];
    int64_t starttime;
    int64_t runtime;
    uint64_t pausedtime;
    uint_fast8_t trackNumber;
    uint_fast8_t volumeNumber;
    bool isPaused = false;
    AudioQualityEnum quality;
    bool loaded = false;


    void setQuality(const std::string &q) {
        if (q == "HI_RES") {
            quality = master;
        } else {
            quality = hifi;
        }
    }


    inline bool isHighRes() const noexcept {
        return quality == master;
    }


    friend std::ostream &operator<<(std::ostream &out, const Song &song) {
        out << song.title << " of " << song.album << " from " << song.artist << "(" << song.runtime << ")";
        return out;
    }
};



std::string urlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::value_type c: value) {
        if (isalnum((unsigned char) c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}


struct Application {
    struct IDiscordCore *core;
    struct IDiscordUsers *users;
};

struct Application app;


static void updateDiscordPresence(const Song &song, std::deque<char> & songDetails, std::deque<char> & songScroll) {
    struct IDiscordActivityManager *manager = app.core->get_activity_manager(app.core);

    if (!song.isPaused) {
        currentTime++;
    }
    if (isPresenceActive && song.loaded) {

        struct DiscordActivity activity{DiscordActivityType_Streaming};
        memset(&activity, 0, sizeof(activity));
        activity.type = DiscordActivityType_Streaming;
        activity.application_id = APPLICATION_ID;

//        for (char i: songDetails) {
//            std::cout << ((int)i) << " ";
//        }

        if (songScroll.size() <= AUTOSCROLL_CAP) {
            songScroll = songDetails;
        } else {
            songScroll.pop_front();
        }
        char buffer[128];
        int counter = 0;
        for (char i: songScroll) {
            buffer[counter] = i;
            counter++;
        }

        snprintf(activity.details, 128, "%s", buffer);





        double currentSegment = static_cast<double>(currentTime) / static_cast<double>(song.runtime);

        int segmentForCircle = (int)(MAX_PLAYER_CHARS_MINUS_ONE * currentSegment);

        char player[MAX_PLAYER_CHARS + 12];

        long long seconds = currentTime % 60;
        long long minutes = currentTime / 60;
        char minutesAsString[3];
        char secondsAsString[3];

        if (minutes < 10) {
            player[0] = '0';
            snprintf(minutesAsString,2,"%d",minutes);
            player[1] = minutesAsString[0];
        } else {
            snprintf(minutesAsString,2,"%d",minutes);
            player[0] = minutesAsString[0];
            player[1] = minutesAsString[1];
        }
        player[2] = ':';
        if (seconds < 10) {
            player[3] = '0';
            snprintf(secondsAsString,3,"%d",seconds);
            player[4] = secondsAsString[0];
        } else {
            snprintf(secondsAsString,3,"%d",seconds);
            player[3] = secondsAsString[0];
            player[4] = secondsAsString[1];
        }
        player[5] = ' ';

        for (int i=0; i < MAX_PLAYER_CHARS; i++) {

            if (i == segmentForCircle) {
                player[(TIME_CHARACTERS/2) + i] = 'o';
            } else {
                player[(TIME_CHARACTERS/2) + i] = '-';
            }

        }
        player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 6] = ' ';

        seconds = song.runtime % 60;
        minutes = song.runtime / 60;
        if (minutes < 10) {
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 5] = '0';
            snprintf(minutesAsString,2,"%d",minutes);
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 4] = minutesAsString[0];
        } else {
            snprintf(minutesAsString,2,"%d",minutes);
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 5] = minutesAsString[0];
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 4] = minutesAsString[1];

        }
        player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 3] = ':';
        if (seconds < 10) {
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 2] = '0';
            snprintf(secondsAsString,3,"%d",seconds);
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 1] = secondsAsString[0];
        } else {
            snprintf(secondsAsString,3,"%d",seconds);
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 2] = secondsAsString[0];
            player[(MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS) - 1] = secondsAsString[1];
        }


        player[MAX_PLAYER_CHARS_MINUS_ONE + TIME_CHARACTERS] = '\0';


        snprintf(activity.state, 128, "%s", player);


        struct DiscordActivityAssets assets{};
        memset(&assets, 0, sizeof(assets));
        if (song.isPaused) {
            snprintf(assets.small_image, 128, "%s", "pause");
            snprintf(assets.small_text, 128, "%s", "Paused");
        }

        snprintf(assets.large_image, 128, "%s", song.isHighRes() ? "test" : HIFI_ASSET);
        snprintf(assets.large_text, 128, "%s", song.isHighRes() ? "Playing High-Res Audio" : "");

        activity.assets = assets;

        activity.instance = false;

        manager->update_activity(manager, &activity, nullptr, nullptr);
    } else {
        std::cout << "Clearing activity" << std::endl;
        manager->clear_activity(manager, nullptr, nullptr);
    }
}


static void discordInit() {
    memset(&app, 0, sizeof(app));

    IDiscordCoreEvents events;
    memset(&events, 0, sizeof(events));

    struct DiscordCreateParams params{};
    params.client_id = APPLICATION_ID;
    params.flags = DiscordCreateFlags_Default;
    params.events = &events;
    params.event_data = &app;

    DiscordCreate(DISCORD_VERSION, &params, &app.core);

    std::lock_guard<std::mutex> lock(currentSongMutex);
    currentStatus = "Connected to Discord";
}


[[noreturn]] inline void rpcLoop(std::deque<char> & songDetails, std::deque<char> & songScroll) {
    using json = nlohmann::json;
    using string = std::string;
    httplib::Client cli("api.tidal.com", 80, 3);
    char getSongInfoBuf[1024];
    json j;
    static Song curSong;

    for (;;) {
        if (isPresenceActive) {
            std::wstring tmpTrack, tmpArtist;
            auto localStatus = tidalInfo(tmpTrack, tmpArtist);

            // If song is playing
            if (localStatus == playing) {
                // if new song is playing
                if (rawWstringToString(tmpTrack) != curSong.title || rawWstringToString(tmpArtist) != curSong.artist) {
                    // assign new info to current track
                    curSong.title = rawWstringToString(tmpTrack);
                    curSong.artist = rawWstringToString(tmpArtist);

                    curSong.runtime = 0;
                    curSong.pausedtime = 0;
                    curSong.setQuality("");
                    curSong.id[0] = '\0';
                    curSong.loaded = true;

                    std::lock_guard<std::mutex> lock(currentSongMutex);
                    currentStatus = "Playing " + curSong.title;

                    // get info form TIDAL api
                    auto search_param =
                        std::string(curSong.title + " - " + curSong.artist.substr(0, curSong.artist.find('&')));

                    sprintf(getSongInfoBuf,
                            "/v1/search?query=%s&limit=50&offset=0&types=TRACKS&countryCode=%s",
                            urlEncode(search_param).c_str(), countryCode ? countryCode : "US");

                    std::clog << "Querying :" << getSongInfoBuf << "\n";

                    httplib::Headers headers = {{
                                                    "x-tidal-token", "CzET4vdadNUFQ5JU"
                                                }};
                    auto res = cli.Get(getSongInfoBuf, headers);

                    if (res && res->status == 200) {
                        try {
                            j = json::parse(res->body);
                            for (auto i = 0u; i < j["tracks"]["totalNumberOfItems"].get<unsigned>(); i++) {
                                // convert title from windows and from tidal api to strings, json lib doesn't support wide string
                                // so wstrings are pared as strings and have the same convention errors
                                auto fetched_str = j["tracks"]["items"][i]["title"].get<std::string>();
                                auto c_str = rawWstringToString(tmpTrack);

                                if (fetched_str == c_str) {
                                    if (curSong.runtime == 0
                                        or j["tracks"]["items"][i]["audioQuality"].get<std::string>().compare("HI_RES")
                                            == 0) {     // Ignore songs with same name if you have found song
                                        curSong.setQuality(j["tracks"]["items"][i]["audioQuality"].get<std::string>());
                                        curSong.trackNumber = j["tracks"]["items"][i]["trackNumber"].get<uint_fast8_t>();
                                        curSong.volumeNumber = j["tracks"]["items"][i]["volumeNumber"].get<uint_fast8_t>();
                                        curSong.runtime = j["tracks"]["items"][i]["duration"].get<int64_t>();
                                        sprintf(curSong.id, "%u", j["tracks"]["items"][i]["id"].get<unsigned>());
                                        if (curSong.isHighRes()) break;     // keep searching for high-res version.
                                    }
                                }
                            }
                        } catch (...) {
                            std::cerr << "Error getting info from api: " << curSong << "\n";
                        }
                    } else {
                        std::cout << "Did not get results\n";
                    }

#ifdef DEBUG
                    std::cout << curSong.title << "\tFrom: " << curSong.artist << std::endl;
#endif
                    std::string details = (curSong.title + " - " + curSong.artist + " - " + curSong.album);
                    unsigned long long length = details.length();

                    char buffer[length + 1];
                    std::strcpy(buffer,details.c_str());
                    songDetails.clear();
                    for (int i=0; i<length+1; i++) {
                        songDetails.push_back(buffer[i]);
                    }

                    songScroll = songDetails;
                    // get time just before passing it to RPC handlers
                    currentTime = 0;
                    curSong.starttime = CURRENT_TIME + 2;  // add 2 seconds to be more accurate, not a chance

                    updateDiscordPresence(curSong,songDetails,songScroll);
                } else {
                    if (curSong.isPaused) {
                        curSong.isPaused = false;
                        updateDiscordPresence(curSong,songDetails,songScroll);

                        std::lock_guard<std::mutex> lock(currentSongMutex);
                        currentStatus = "Paused " + curSong.title;
                    } else {
                        updateDiscordPresence(curSong,songDetails,songScroll);
                    }
                }

            } else if (localStatus == opened) {
                curSong.pausedtime += 1;
                curSong.isPaused = true;
                updateDiscordPresence(curSong,songDetails,songScroll);

                std::lock_guard<std::mutex> lock(currentSongMutex);
                currentStatus = "Paused " + curSong.title;
            } else {
                curSong = Song();
                updateDiscordPresence(curSong,songDetails,songScroll);

                std::lock_guard<std::mutex> lock(currentSongMutex);
                currentStatus = "Waiting for Tidal";
            }
        } else {
            curSong = Song();
            updateDiscordPresence(curSong,songDetails,songScroll);

            std::lock_guard<std::mutex> lock(currentSongMutex);
            currentStatus = "Disabled";
        }

        enum EDiscordResult result = app.core->run_callbacks(app.core);
        if (result != DiscordResult_Ok) {
            std::cout << "Bad result " << result << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    }
}


int main(int argc, char **argv) {

    // get country code for TIDAL api queries
    countryCode = getLocale();
    isPresenceActive = true;

    // Qt main app setup
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":assets/icon.ico"));

    QSystemTrayIcon tray(QIcon(":assets/icon.ico"), &app);
    QAction titleAction(QIcon(":assets/icon.ico"), "TIDAL - Discord RPC ", nullptr);
    QAction changePresenceStatusAction("Running", nullptr);
    changePresenceStatusAction.setCheckable(true);
    changePresenceStatusAction.setChecked(true);
    QObject::connect(&changePresenceStatusAction,
                     &QAction::triggered,
                     [&changePresenceStatusAction]() {
                       isPresenceActive = !isPresenceActive;
                       changePresenceStatusAction.setText(isPresenceActive ? "Running"
                                                                           : "Disabled (click to re-enable)");
                     }
    );
    std::deque<char> songDetails;
    std::deque<char> songScroll;
    QAction quitAction("Exit", nullptr);
    QObject::connect(&quitAction, &QAction::triggered, [&app, &songScroll, &songDetails]() {
        updateDiscordPresence(Song(),songDetails,songScroll);
      app.quit();
    });

    QAction currentlyPlayingAction("Status: waiting", nullptr);
    currentlyPlayingAction.setDisabled(true);

    QMenu trayMenu("TIDAL - RPC", nullptr);

    trayMenu.addAction(&titleAction);
    trayMenu.addAction(&changePresenceStatusAction);
    trayMenu.addAction(&currentlyPlayingAction);
    trayMenu.addAction(&quitAction);

    tray.setContextMenu(&trayMenu);

    tray.show();

    #if defined(__APPLE__) or defined(__MACH__)

    if (!macPerms()){
        std::cerr << "No Screen Recording Perms \n";
    }

    #endif

    QTimer timer(&app);
    QObject::connect(&timer, &QTimer::timeout, &app, [&currentlyPlayingAction]() {
      std::lock_guard<std::mutex> lock(currentSongMutex);
      currentlyPlayingAction.setText("Status: " + QString(currentStatus.c_str()));
    });
    timer.start(1000);

    QObject::connect(&app, &QApplication::aboutToQuit, [&timer]() {
      timer.stop();
    });

    discordInit();


    // RPC loop call
    std::thread t1(rpcLoop,std::ref(songDetails),std::ref(songScroll));
    t1.detach();

    return app.exec();

}
