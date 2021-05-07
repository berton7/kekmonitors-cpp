#pragma once
#include <kekmonitors/core.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace kekmonitors {

enum COMMANDS : uint32_t {
    PING = 1,
    STOP,
    SET_SHOES,
    ADD_SHOES,
    GET_SHOES,
    GET_CONFIG,
    GET_WHITELIST,
    GET_BLACKLIST,
    GET_WEBHOOKS,
    SET_SPECIFIC_CONFIG,
    SET_SPECIFIC_WEBHOOKS,
    SET_SPECIFIC_BLACKLIST,
    SET_SPECIFIC_WHITELIST,
    SET_COMMON_CONFIG,
    SET_COMMON_WEBHOOKS,
    SET_COMMON_BLACKLIST,
    SET_COMMON_WHITELIST,

    MM_ADD_MONITOR,
    MM_ADD_SCRAPER,
    MM_ADD_MONITOR_SCRAPER,
    MM_STOP_MONITOR,
    MM_STOP_SCRAPER,
    MM_STOP_MONITOR_SCRAPER,
    MM_STOP_MONITOR_MANAGER,
    MM_GET_MONITOR_STATUS,
    MM_GET_SCRAPER_STATUS,
    MM_GET_MONITOR_SCRAPER_STATUS,
    MM_SET_MONITOR_CONFIG,
    MM_GET_MONITOR_CONFIG,
    MM_SET_MONITOR_WHITELIST,
    MM_GET_MONITOR_WHITELIST,
    MM_SET_MONITOR_BLACKLIST,
    MM_GET_MONITOR_BLACKLIST,
    MM_SET_MONITOR_WEBHOOKS,
    MM_GET_MONITOR_WEBHOOKS,
    MM_SET_SCRAPER_CONFIG,
    MM_GET_SCRAPER_CONFIG,
    MM_SET_SCRAPER_WHITELIST,
    MM_GET_SCRAPER_WHITELIST,
    MM_SET_SCRAPER_BLACKLIST,
    MM_GET_SCRAPER_BLACKLIST,
    MM_SET_SCRAPER_WEBHOOKS,
    MM_GET_SCRAPER_WEBHOOKS,
    MM_SET_MONITOR_SCRAPER_BLACKLIST,
    MM_SET_MONITOR_SCRAPER_WHITELIST,
    MM_SET_MONITOR_SCRAPER_WEBHOOKS,
    MM_SET_MONITOR_SCRAPER_CONFIG,
    MM_GET_MONITOR_SHOES,
    MM_GET_SCRAPER_SHOES,
};

enum ERRORS : uint32_t {
    OK = 0,

    SOCKET_DOESNT_EXIST,
    SOCKET_COULDNT_CONNECT,
    SOCKET_TIMEOUT,

    MONITOR_DOESNT_EXIST,
    SCRAPER_DOESNT_EXIST,
    MONITOR_NOT_REGISTERED,
    SCRAPER_NOT_REGISTERED,

    UNRECOGNIZED_COMMAND,
    BAD_PAYLOAD,
    MISSING_PAYLOAD,
    MISSING_PAYLOAD_ARGS,

    MM_COULDNT_ADD_MONITOR,
    MM_COULDNT_ADD_SCRAPER,
    MM_COULDNT_ADD_MONITOR_SCRAPER,
    MM_COULDNT_STOP_MONITOR,
    MM_COULDNT_STOP_SCRAPER,
    MM_COULDNT_STOP_MONITOR_SCRAPER,

    OTHER_ERROR,
    UNKNOWN_ERROR
};

class IMessage {
  public:
    virtual bool fromJson(const json &obj) = 0;
    virtual json toJson() = 0;
};

class Cmd : public IMessage {
  protected:
    COMMANDS _cmd;
    json _payload;

  public:
    Cmd();
    ~Cmd();

    bool fromJson(const json &obj) override;

    json toJson() override;

    COMMANDS getCmd() const;
    void setCmd(COMMANDS cmd);
    const json &getPayload() const;
    void setPayload(const json &payload);
};

class Response : public IMessage {
  private:
    ERRORS _error;
    std::string _info;
    json _payload;

  public:
    Response();
    ~Response();

    bool fromJson(const json &obj) override;
    json toJson() override;
    ERRORS getError() const;
    void setError(ERRORS error);
    const json &getPayload() const;
    void setPayload(const json &payload);
    const std::string &getInfo() const;
    void setInfo(const std::string &info);

    static Response okResponse();
    static Response badResponse();
};
} // namespace kekmonitors