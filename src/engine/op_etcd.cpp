#include "op_etcd.h"

std::string getCmdResult(const std::string &strCmd)
{
    char buf[10240] = {0};
    FILE *pf = NULL;

    if( (pf = popen(strCmd.c_str(), "r")) == NULL )
    {
        return "";
    }

    std::string strResult;
    while(fgets(buf, sizeof buf, pf))
    {
        strResult += buf;
    }

    pclose(pf);

    unsigned int iSize =  strResult.size();
    if(iSize > 0 && strResult[iSize - 1] == '\n')  // linux
    {
        strResult = strResult.substr(0, iSize - 1);
    }

    return strResult;
}


// 覆盖写
bool etcd_put(std::string key, std::string val){
    std::string tar = "etcdctl";
    std::string strcmd = tar + " " + "put " + key + " " + "\"" + val + "\"";
    std::string ret = getCmdResult(strcmd);
    if(ret == "OK")
        return true;
    return false;
}

std::string etcd_get(std::string key){
    std::string tar = "etcdctl";
    std::string strcmd = tar + " " + "get " + key + " --print-value-only";
    std::string ret = getCmdResult(strcmd);
    return ret;
}

bool etcd_del(std::string key){
    std::string tar = "etcdctl";
    std::string strcmd = tar + " " + "del " + key;
    std::string ret = getCmdResult(strcmd);
    if(ret == "1")
        return true;
    return false;
}

std::map<std::string, std::string> etcd_par(std::string key){
    std::string tar = "etcdctl";
    std::string strcmd = tar + " " + "get " + "--prefix "+ key;
    std::string ret = getCmdResult(strcmd);
    std::map<std::string, std::string> res;

    std::stringstream str;
    str << ret;
    std::string k,v;
    while(str >> k){
        char tmp;
        str.get(tmp);// 处理回车
        std::getline(str,v);
        res[k] = v;
    }

    for(auto i:res){
        std::cout << i.first << " " << i.second << std::endl; 
    }
    return res;
}

std::vector<std::string> etcd_get_key(std::string prefix){
    std::string tar = "etcdctl";
    std::string strcmd = tar + " " + "get " + "--prefix "+ prefix;
    std::string ret = getCmdResult(strcmd);
    std::map<std::string, std::string> mp;

    std::stringstream str;
    str << ret;
    std::string k,v;
    while(str >> k){
        char tmp;
        str.get(tmp);// 处理回车
        std::getline(str,v);
        mp[k] = v;
    }

    std::vector<std::string> res;
    for(auto i:mp){
        std::string tmp = (i.first).substr(prefix.length());
        std::cout <<tmp << std::endl;
        res.push_back(tmp);
    }
    return res;
}