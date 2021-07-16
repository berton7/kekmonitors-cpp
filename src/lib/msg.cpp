#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

namespace kekmonitors {

Cmd::Cmd() : _cmd(COMMANDS::PING){};
Cmd::~Cmd() = default;

Cmd Cmd::fromJson(const json &obj) {
    Cmd cmd;
    bool success = false;
    try {
        cmd._cmd = obj.at("_Cmd__cmd");
    } catch (json::exception &e) {
        throw std::invalid_argument("Json object doesn't contain \"_Cmd__cmd\"");
    }
    try {
        cmd._payload = obj.at("_Cmd__payload");
    } catch (json::exception &e) {
    };
    return cmd;
};

Cmd Cmd::fromJson(const json &obj, error_code &ec)
{
    Cmd cmd;
    try {
        cmd._cmd = obj.at("_Cmd__cmd");
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
    }
    try {
        cmd._payload = obj.at("_Cmd__payload");
    } catch (json::exception &e) {
    };
    return cmd;
}

json Cmd::toJson() const {
    json j;
    j["_Cmd__cmd"] = _cmd;
    if (!_payload.empty())
        j["_Cmd__payload"] = _payload;
    return j;
};

Cmd Cmd::fromString(const std::string &str) {
    return fromJson(json::parse(str));
}

Cmd Cmd::fromString(const std::string &str, error_code &ec) {
    try {
        return fromJson(json::parse(str), ec);
    }
    catch(json::exception &e)
    {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
        return Cmd();
    }
}

std::string Cmd::toString() const { return toJson().dump(); }

kekmonitors::CommandType Cmd::getCmd() const { return _cmd; }
void Cmd::setCmd(kekmonitors::CommandType cmd) { _cmd = cmd; }
const json &Cmd::getPayload() const { return _payload; }
void Cmd::setPayload(const json &payload) { _payload = payload; }

Response::Response() : _error(ERRORS::OK){};
Response::~Response() = default;

Response Response::fromJson(const json &obj) {
    Response response;
    try {
        response._error = obj.at("_Response__error");
    } catch (json::exception &e) {
        throw std::invalid_argument("Json object doesn't contain \"_Response__error\"");
    };
    try {
        response._payload = obj.at("_Response__payload");
    } catch (json::exception &e) {
    };
    try {
        response._info = obj.at("_Response__info");
    } catch (json::exception &e) {
    };
    return response;
};

Response Response::fromJson(const json &obj, error_code &ec) {
    Response response;
    try {
        response._error = obj.at("_Response__error");
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
    };
    try {
        response._payload = obj.at("_Response__payload");
    } catch (json::exception &e) {
    };
    try {
        response._info = obj.at("_Response__info");
    } catch (json::exception &e) {
    };
    return response;
};

json Response::toJson() const {
    json j;
    j["_Response__error"] = _error;
    if (!_info.empty())
        j["_Response__info"] = _info;
    if (!_payload.empty())
        j["_Response__payload"] = _payload;
    return j;
};
kekmonitors::ErrorType Response::getError() const { return _error; }
void Response::setError(kekmonitors::ErrorType error) { _error = error; }
const json &Response::getPayload() const { return _payload; }
void Response::setPayload(const json &payload) { _payload = payload; }
const std::string &Response::getInfo() const { return _info; }
void Response::setInfo(const std::string &info) { _info = info; }

Response Response::okResponse() {
    Response resp;
    resp.setError(ERRORS::OK);
    return resp;
}
Response Response::badResponse() {
    Response resp;
    resp.setError(ERRORS::OTHER_ERROR);
    return resp;
}
std::string Response::toString() const { return toJson().dump(); }
Response Response::fromString(const std::string &str) {
    return fromJson(json::parse(str));
}

Response Response::fromString(const std::string &str, error_code &ec)
{
    try {
        return fromJson(json::parse(str), ec);
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument);
        return Response();
    }
}
}; // namespace kekmonitors
