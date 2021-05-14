#pragma once
#include <kekmonitors/core.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace kekmonitors {

class IMessage {
  public:
    virtual bool fromJson(const json &obj) = 0;
    virtual json toJson() = 0;
    virtual bool fromString(const std::string &str) = 0;
    virtual std::string toString() = 0;
};

class Cmd : public IMessage {
  protected:
    kekmonitors::CommandType _cmd;
    json _payload;

  public:
    Cmd();
    ~Cmd();

    bool fromJson(const json &obj) override;
    json toJson() override;
    bool fromString(const std::string &str) override;
    std::string toString() override;

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

    bool fromJson(const json &obj) override;
    json toJson() override;
    bool fromString(const std::string &str) override;
    std::string toString() override;

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