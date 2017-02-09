#include <cstdio>
#include <string>
#include <map>
#include <sys/ioctl.h>
#include <cassert>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ncurses.h>

/**
 * The maximum size of a datagram we can receive
 */
static const size_t BUFFER_SIZE = 65536;

/**
 * The buffer receiving the datagram
 */
char buffer[BUFFER_SIZE];

/**
 * The window to draw the output to
 */
WINDOW* window = nullptr;

/**
 * A type for metrics
 */
enum class MetricType
{
    Unknown, /// Some format error
    Count, /// Corresponds to statsd.increment
    Gauge, /// Corresponds to statsd.gauge 
    Timing, /// Corresponds to statsd.duration
};

/** 
 * Things that are weird will be emitted as a counter with this name
 */
static const std::string unknown_name = "<<<unknown>>>";

/**
 * A metric container class for keeping running counts, etc 
 */
struct Metric
{
    MetricType type;
    size_t count;
    double value;
    
    Metric(MetricType type = MetricType::Unknown) : type(type), count(0), value(0.0)
    {
    }
    
    /**
     * Updates the metric with a measurement
     */
    void update(std::string str = "1")
    {
        double in_d = std::stod(str.c_str());
        
        switch(type)
        {
            case MetricType::Unknown:
            case MetricType::Count:
                count += 1;
                break;
            case MetricType::Gauge:
                count += 1;
                value = in_d;
                break;
            case MetricType::Timing:
                count += 1;
                value += in_d;
                break;
            default:
                break;
        }
    }
    
    /**
     * Compute a printable string from the metric
     */
    std::string to_string()
    {
        switch(type)
        {
            case MetricType::Unknown:
            case MetricType::Count:
                return std::to_string(count);
            case MetricType::Gauge:
                return std::to_string(count) + " @ " + std::to_string(value);
            case MetricType::Timing:
                return std::to_string(count) + " @ " + std::to_string(value);
        }
        return "";
    }
};


/**
 * Typedefs for socket structs
 */
typedef struct sockaddr SocketAddress;
typedef struct sockaddr_in InSocketAddress;

/**
 * name -> Metric sorted by name
 */
std::map<std::string, Metric> metrics;

/**
 * Prints the state of the metrics
 */
void print_metrics()
{
    werase(window);
    for(auto& metric : metrics)
    {
        std::string out = metric.first + ": " + metric.second.to_string();
        wprintw(window, "%s\n", out.c_str());
    }
    wrefresh(window);
}

/**
 * The parsing state is kepted in this enum
 */
enum class IOState
{
    ReadingName, // Ended by :
    ReadingMeasurement, // Ended by |
    ReadingType, // Expecting a m, g or c
    ReadingTypeM, // Expecting an s
    ReadingTypeMS, // Expecting an | or EOF
    ReadingTypeG, // Expecting an | or EOF
    ReadingTypeC, // Expecting an | or EOF
    ReadingTags, /// Ended by EOF
};


int main(int argc, char** argv)
{
    /**
     * Creates a UDP server listening to statsd's port
     */
    auto server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    InSocketAddress server_address;
    InSocketAddress client_address;
    socklen_t address_size = sizeof(client_address);
    
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8125);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(server_socket, (SocketAddress*)&server_address, sizeof(server_address));
    
    /**
     * Initializes ncurses
     */
    initscr();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    window = newwin(LINES, COLS, 0, 0);
    wbkgd(window, COLOR_PAIR(1));
    wrefresh(window);
    scrollok(window, true);
    atexit([](){endwin();});
    
    /**
     * Logic loop
     */
    while(true)
    {   
        /**
        * Keep reading datagrams
        */
        auto size = recvfrom(server_socket, buffer, BUFFER_SIZE, 0, (SocketAddress*)&client_address, &address_size);
        std::string name;
        std::string measurement;
        std::string tags;
        MetricType type = MetricType::Unknown;
        IOState state = IOState::ReadingName;
        
        /**
         * Parse the packet
         */
        for(int i = 0; i < size; ++i)
        {
            auto chr = buffer[i];
            switch(state)
            {
                case IOState::ReadingName:
                    if(chr == ':')
                        state = IOState::ReadingMeasurement;
                    else
                        name.push_back(chr);
                    break;
                case IOState::ReadingMeasurement:
                    if(chr == '|')
                        state = IOState::ReadingType;
                    else
                        measurement.push_back(chr);
                    break;
                case IOState::ReadingType:
                    if(chr == 'c')
                    {
                        state = IOState::ReadingTypeC;
                        type = MetricType::Count;
                    }
                    else if(chr == 'g')
                    {
                        state = IOState::ReadingTypeG;
                        type = MetricType::Gauge;
                    }
                    else if(chr == 'm')
                    {
                        state = IOState::ReadingTypeM;
                    }
                    else
                        goto end;
                    break;
                case IOState::ReadingTypeM:
                    if(chr != 's')
                        goto end;
                    state = IOState::ReadingTypeMS;
                    type = MetricType::Timing;
                    break;
                case IOState::ReadingTypeMS:
                case IOState::ReadingTypeC:
                case IOState::ReadingTypeG:
                    if(chr != '|')
                    {
                        type = MetricType::Unknown;
                        goto end;
                    }
                    else
                    {
                        state = IOState::ReadingTags;
                        tags.push_back(buffer[i++]);
                    }
                    break;
                case IOState::ReadingTags:
                    tags.push_back(chr);
                    break;
                    
            }
        }
        end:;
        /**
         * Update and print metrics
         */
        if(type != MetricType::Unknown)
        {
            auto metric_name = name + tags;
            metrics.emplace(metric_name, type);
            metrics[metric_name].update(measurement);
        }
        else
        {
            metrics.emplace(unknown_name, MetricType::Count);
            metrics[unknown_name].update();
        }
        print_metrics();
    }
    
    return 0;
}
