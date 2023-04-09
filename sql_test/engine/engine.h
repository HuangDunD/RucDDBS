#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include "parser/ast.h"
#include "parser/parser.h"
#include "record.h"
#include "dbconfig.h"
#include "meta_service.pb.h"
#include "meta_server.h"
#include "kv_store.h"
using namespace std;
#define debug_test 1

class Operators{
public:
    shared_ptr<Operators> next_node = nullptr;
    virtual vector<shared_ptr<record>> exec_op() = 0;
    void prt_op(){
        if(debug_test)
            cout << typeid(*this).name() << endl;
    }
};

vector<shared_ptr<record>> send_plan(string target_address, shared_ptr<Operators> node_plan);
vector<shared_ptr<record>> Sql_execute(string str);
void test_init_0(vector<shared_ptr<record>> &ret);
void test_init_1(vector<shared_ptr<record>> &ret);

class op_executor: public Operators{
public:
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        return next_node->exec_op();
    }
};

class op_projection: public Operators{
public:
    vector<shared_ptr<ast::Col>> cols;

    vector<shared_ptr<record>> exec_op(){
        prt_op();
        vector<shared_ptr<record>> res;
        auto ret = next_node->exec_op();
        for(int i = 0; i < ret.size() ; i++){
            auto cur_record = ret[i];
            shared_ptr<record> new_record(new record);
            for(int j = 0; j < cur_record->row.size(); j++){
                auto cur_value = cur_record->row[j];
                for(int k = 0; k < cols.size(); k++){
                    if(cur_value->col_name == cols[k]->col_name)
                        new_record->row.push_back(cur_value);
                }
            }
            res.push_back(new_record);
        }

        return res;
    }
};

class op_selection: public Operators{
public:
    std::shared_ptr<ast::BinaryExpr> conds;
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        auto ret = next_node->exec_op();
        vector<shared_ptr<record>> res;

        if(auto x = std::dynamic_pointer_cast<ast::Col>(conds->rhs)){
            int lf_index = 0;
            int rg_index = 0;
            for(int i = 0; i < ret[0]->row.size(); i++){
                if(ret[0]->row[i]->tab_name == conds->lhs->tab_name && ret[0]->row[i]->col_name == conds->lhs->col_name){
                    lf_index = i;
                }
                if(ret[0]->row[i]->tab_name == x->tab_name && ret[0]->row[i]->col_name == x->col_name){
                    rg_index = i;
                }
            }

            for(int i = 0; i < ret.size(); i++){
                if(ret[i]->row[lf_index]->str == ret[i]->row[rg_index]->str){
                    res.push_back(ret[i]);
                }
            }
        }
        else if(auto x = std::dynamic_pointer_cast<ast::IntLit>(conds->rhs)){
            int lf_index = 0;
            for(int i = 0; i < ret[0]->row.size(); i++){
                if(ret[0]->row[i]->tab_name == conds->lhs->tab_name && ret[0]->row[i]->col_name == conds->lhs->col_name){
                    lf_index = i;
                }
            }
            
            for(int i = 0; i < ret.size(); i++){
                if(ret[i]->row[lf_index]->str == (x->val)){
                    res.push_back(ret[i]);
                }
            }
        }

        return res;
    }
};

class op_insert: public Operators{
public:
    string tab_name;
    int par;
    shared_ptr<record> rec;
    
    vector<shared_ptr<record>> exec_op(){
        // 默认第一列为主键
        string key = tab_name + to_string(par) + to_string(rec->row[0]->str);
        // value
        put(key, rec);
    }
};

class op_delete: public Operators{
public:
    string tab_name;
    int par;
    vector<shared_ptr<record>> exec_op(){
        auto res = next_node->exec_op();
        // 构造key 进行删除
        for(auto iter: res){
            auto key = tab_name + to_string(par) + to_string(iter->row[0]->str);
            del(key);
        }
    }
};

class op_update: public Operators{
public:
    string tab_name;
    int par;
    
    vector<int> set_col_index;
    vector<shared_ptr<value>> set_val;
    vector<shared_ptr<record>> exec_op(){
        auto res = next_node->exec_op();
        // 构造key 进行更新
        for(auto iter: res){
            auto key = tab_name + to_string(par) + to_string(iter->row[0]->str);
            auto row = iter->row;
            for(int i = 0 ; i < int(set_col_index.size()); i++){
                int col = set_col_index[i];
                auto val = set_val[i];
                row[col]->str = val->str;
            }
            put(key, iter);
        }
    }
};

class op_distribution: public Operators{
public:
    vector<shared_ptr<Operators>> nodes_plan;
    vector<string> target_address;

    vector<shared_ptr<record>> exec_op(){
        prt_op();
        vector<shared_ptr<record>> res;

        for(int i = 0 ; i < nodes_plan.size(); i++){
            auto ret = send_plan(target_address[i],nodes_plan[i]);
            res.insert(res.end(), ret.begin(), ret.end());
        }

        return res;
    }
    
};

class op_join: public Operators{
public:
    vector<std::shared_ptr<Operators>> tables_get;

    vector<shared_ptr<record>> exec_op(){
        // 
        vector<vector<shared_ptr<record>>> ret;
        for(int i = 0; i < tables_get.size(); i++){
            ret.push_back(tables_get[i]->exec_op());
        }

        auto res = ret[0];
        vector<shared_ptr<record>> tmp; 

        for(int i = 1; i < ret.size(); i++){
            auto tab = ret[i];
            for(auto j = 0; j < res.size(); j++){
                for(auto k = 0; k < tab.size(); k++){
                    shared_ptr<record> join_record(new record);
                    join_record->row.insert(join_record->row.end(), res[j]->row.begin(), res[j]->row.end());
                    join_record->row.insert(join_record->row.end(), tab[k]->row.begin(), tab[k]->row.end());
                    tmp.push_back(join_record);
                }
            }
            res = tmp;
        }

        return res;
    }
};



class op_tablescan: public Operators{
public:
    string tabs;
    int par;
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        vector<shared_ptr<record>> res;
        get_par(tabs, par, res);
        return res;
    }
};
