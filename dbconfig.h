#pragma once
#ifndef DBCONFIG_H
#define DBCONFIG_H

#include <string>
#include <gflags/gflags.h>

static const std::string DATA_DIR = "/home/t500ttt/RucDDBS/data/";
static const std::string META_SERVER_FILE_NAME = "META_SERVER.meta";
static const int META_SERVER_PORT = 8001;
static const std::string META_SERVER_LISTEN_ADDR = "[::0]:8001";
static const int idle_timeout_s = -1;


// log path
static const std::string log_path = "/home/t500ttt/RucDDBS/data/";

// DEFINE_int32(META_SERVER_PORT, 8001, "Meta Server port, if Meta server listen address is not set, meta server " 
//             "will listen any address in this port");
// DEFINE_string(META_SERVER_LISTEN_ADDR,  "[::0]:8001", "Meta Server listen address, may be IPv4/IPv6");
// DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
//              "read/write operations during the last `idle_timeout_s'");


#endif