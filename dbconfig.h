#pragma once
#ifndef DBCONFIG_H
#define DBCONFIG_H

#include <string>
#include <gflags/gflags.h>

//config for metaserver
static const std::string DATA_DIR = "/home/t500ttt/RucDDBS/data/";
static const std::string META_SERVER_FILE_NAME = "META_SERVER.meta";
static const int META_SERVER_PORT = 8001; // maybe not used
static const std::string META_SERVER_LISTEN_ADDR = "0.0.0.0:8001";
static const int idle_timeout_s = -1;

DECLARE_string(META_SERVER_ADDR);

DECLARE_int32(SERVER_PORT);
DECLARE_string(SERVER_LISTEN_ADDR);

// log path
static const std::string log_path = "/home/t500ttt/RucDDBS/data/";

#endif