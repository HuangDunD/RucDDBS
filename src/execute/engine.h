#pragma once

#include "head.h"


class Operators{
public:
    shared_ptr<Operators> next_node;
    shared_ptr<KV> kv;
    virtual vector<shared_ptr<record>> exec_op() = 0;
};

class op_excutor: public Operators{
public:
    vector<shared_ptr<record>> exec_op(){
        return next_node->exec_op();
    }
};

class op_projection: public Operators{
public:
    vector<shared_ptr<ast::Col>> cols;

    vector<shared_ptr<record>> exec_op(){
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
                if(atoi(ret[i]->row[lf_index]->str.c_str()) == (x->val)){
                    res.push_back(ret[i]);
                }
            }
        }

        return res;
    }
};

class op_distribution: public Operators{
public:
    vector<shared_ptr<Operators>> nodes_plan;
    vector<string> target_address;

    vector<shared_ptr<record>> exec_op(){
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
        vector<shared_ptr<record>> res;

        for(auto i: kv->Key_range()){
            shared_ptr<record> red(new record);
            kv->get(i,red);
            res.push_back(red);
        }

        return res;
    }
};

vector<shared_ptr<record>> send_plan(string target_address, shared_ptr<Operators> node_plan);
vector<shared_ptr<record>> Sql_execute(string str);