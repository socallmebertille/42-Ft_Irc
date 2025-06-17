#ifndef MODECOMMANDS_HPP
# define MODECOMMANDS_HPP

# include "Server.hpp"
# include "Channel.hpp"
# include "Client.hpp"
# include <vector>
# include <string>

class Server; // Forward declaration

class ModeCommands {
public:
    static void mode(Server* server);

private:
    static bool validateModeCommand(Server* server, const std::string& target, Channel*& chan);
    static void showCurrentModes(Server* server, const std::string& channelName, const Channel& chan);
    static bool processSingleMode(Server* server, char flag, bool adding, const std::vector<std::string>& params,
                                 size_t& paramIndex, Channel& chan, const std::string& channelName,
                                 std::string& appliedModes, std::string& appliedParams);
    static bool handleOperatorMode(Server* server, bool adding, const std::vector<std::string>& params, size_t& paramIndex,
                                  Channel& chan, const std::string& channelName,
                                  std::string& appliedModes, std::string& appliedParams);
};

#endif
