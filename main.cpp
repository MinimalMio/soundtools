#include <SFML/Audio.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <sstream>

struct Note {
    int delta_time;
    double frequency;
    int duration;
};

struct Track {
    std::string name;
    std::vector<Note> notes;
    std::vector<double> start_times_ms;
};

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}
void computeStartTimes(Track& track) {
    if (track.notes.empty()) return;
    track.start_times_ms.clear();
    
    track.start_times_ms.push_back(track.notes[0].delta_time);
    
    for (size_t i = 1; i < track.notes.size(); ++i) {
        double prev_end = track.start_times_ms[i-1] + track.notes[i-1].duration;
        track.start_times_ms.push_back(prev_end + track.notes[i].delta_time);
    }
}

std::vector<Track> parseInput(const std::string& filename) {
    std::vector<Track> tracks;
    std::ifstream file(filename);
    std::string line;
    
    Track current_track;
    bool in_track = false;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (!in_track) {
            if (line.find("track") == 0) {
                size_t brace_pos = line.find('{');
                if (brace_pos == std::string::npos) {
                    std::cerr << "Syntax error: Missing '{' in track definition" << std::endl;
                    continue;
                }

                current_track.name = trim(line.substr(5, brace_pos - 5));
                current_track.notes.clear();
                in_track = true;
            }
        } else {
            if (line.find('}') != std::string::npos) {
                if (!current_track.notes.empty()) {
                    computeStartTimes(current_track);
                    tracks.push_back(current_track);
                } else {
                    std::cerr << "Track " << current_track.name << " has no notes. Skipping." << std::endl;
                }
                in_track = false;
                continue;
            }

            if (line[0] != '[') {
                std::cerr << "Invalid note format: " << line << std::endl;
                continue;
            }

            std::string note_str = line.substr(1, line.find(']') - 1);
            std::replace(note_str.begin(), note_str.end(), ',', ' ');
            
            std::istringstream iss(note_str);
            int delta, duration;
            double freq;
            
            if (!(iss >> delta >> freq >> duration)) {
                std::cerr << "Failed to parse note: " << line << std::endl;
                continue;
            }
            
            current_track.notes.push_back({delta, freq, duration});
        }
    }

    return tracks;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    auto tracks = parseInput(argv[1]);
    if (tracks.empty()) {
        std::cerr << "No valid tracks found!" << std::endl;
        return 1;
    }

    double max_end_time = 0;
    for (const auto& track : tracks) {
        for (size_t i = 0; i < track.notes.size(); ++i) {
            double end_time = track.start_times_ms[i] + track.notes[i].duration;
            max_end_time = std::max(max_end_time, end_time);
        }
    }

    const int sample_rate = 44100;
    const double max_amplitude = 32767 * 0.2;
    const size_t total_samples = static_cast<size_t>(max_end_time * sample_rate / 1000.0);
    std::vector<short> buffer(total_samples, 0);

    for (const auto& track : tracks) {
        for (size_t i = 0; i < track.notes.size(); ++i) {
            const auto& note = track.notes[i];
            double start_time = track.start_times_ms[i];
            size_t start_sample = static_cast<size_t>(start_time * sample_rate / 1000.0);
            size_t duration_samples = static_cast<size_t>(note.duration * sample_rate / 1000.0);

            for (size_t s = 0; s < duration_samples; ++s) {
                if (start_sample + s >= buffer.size()) break;
                
                double t = static_cast<double>(s) / sample_rate;
                double value = sin(2 * M_PI * note.frequency * t) * max_amplitude;
                
                buffer[start_sample + s] += static_cast<short>(value);
                
                buffer[start_sample + s] = std::max<short>(-32768, 
                                         std::min<short>(32767, 
                                         buffer[start_sample + s]));
            }
        }
    }

    sf::SoundBuffer soundBuffer;
    if (!soundBuffer.loadFromSamples(buffer.data(), buffer.size(), 1, sample_rate)) {
        std::cerr << "Failed to create sound buffer!" << std::endl;
        return 1;
    }

    sf::Sound sound(soundBuffer);
    sound.play();

    while (sound.getStatus() == sf::Sound::Playing) {
        sf::sleep(sf::milliseconds(100));
    }

    return 0;
}