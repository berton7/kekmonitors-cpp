#pragma once
#include <kekmonitors/core.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace kekmonitors {

class IMessage {
  public:
    virtual json toJson() const = 0;
    virtual std::string toString() const = 0;
};

class Cmd : public IMessage {
  protected:
    kekmonitors::CommandType _cmd;
    json _payload;

  public:
    Cmd();
    ~Cmd();

    static Cmd fromJson(const json &obj);
    static Cmd fromJson(const json &obj, error_code &ec);
    json toJson() const override;
    static Cmd fromString(const std::string &str);
    static Cmd fromString(const std::string &str, error_code &ec);
    std::string toString() const override;

    kekmonitors::CommandType getCmd() const;
    void setCmd(kekmonitors::CommandType cmd);
    const json &getPayload() const;
    void setPayload(const json &payload);
};

class Response : public IMessage {
  private:
    kekmonitors::ErrorType _error;
    std::string _info;
    json _payload;

  public:
    Response();
    ~Response();

    static Response fromJson(const json &obj);
    static Response fromJson(const json &obj, error_code &ec);
    json toJson() const override;
    static Response fromString(const std::string &str);
    static Response fromString(const std::string &str, error_code &ec);
    std::string toString() const override;

    kekmonitors::ErrorType getError() const;
    void setError(kekmonitors::ErrorType error);
    const json &getPayload() const;
    void setPayload(const json &payload);
    const std::string &getInfo() const;
    void setInfo(const std::string &info);

    static Response okResponse();
    static Response badResponse();
};
} // namespace kekmonitors