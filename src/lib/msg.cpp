#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

namespace kekmonitors {

Cmd::Cmd() : _cmd(COMMANDS::PING){};
Cmd::~Cmd() = default;

bool Cmd::fromJson(const json &obj) {
  bool success = false;
  try {
    _cmd = obj.at("_Cmd__cmd");
    success = true;
  } catch (json::exception &e) {
  }
  try {
    _payload = obj.at("_Cmd__payload");
  } catch (json::exception &e) {
  };
  return success;
};

json Cmd::toJson() {
  json j;
  j["_Cmd__cmd"] = _cmd;
  if (!_payload.empty())
    j["_Cmd__payload"] = _payload;
  return j;
};

COMMANDS Cmd::getCmd() const { return _cmd; }
void Cmd::setCmd(COMMANDS cmd) { _cmd = cmd; }
const json &Cmd::getPayload() const { return _payload; }
void Cmd::setPayload(const json &payload) { _payload = payload; }

Response::Response() : _error(ERRORS::OK){};
Response::~Response() = default;

bool Response::fromJson(const json &obj) {
  bool success = false;
  try {
    _error = obj.at("_Response__error");
    success = true;
  } catch (json::exception &e) {
  };
  try {
    _payload = obj.at("_Response__payload");
  } catch (json::exception &e) {
  };
  try {
    _info = obj.at("_Response__info");
  } catch (json::exception &e) {
  };
  return success;
};
json Response::toJson() {
  json j;
  j["_Response__error"] = _error;
  if (!_info.empty())
    j["_Response__info"] = _info;
  if (!_payload.empty())
    j["_Response__value"] = _payload;
  return j;
};
ERRORS Response::getError() const { return _error; }
void Response::setError(ERRORS error) { _error = error; }
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
}; // namespace kekmonitors
