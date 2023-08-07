#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include "include/parser/ast.h"
#include "parser.h"
#include "record.h"
#include "kv_store.h"
#include "meta_data.h"
#include <iomanip>
#include "session.pb.h"
#include "transaction_manager.h"
# define DB_NAME "RucDDBS"
using namespace std;
#define debug_test 1

class Context {
public:
    Context (Transaction *txn, TransactionManager *transaction_manager): 
        transaction_manager_(transaction_manager), txn_(txn) { }
    TransactionManager *transaction_manager_;
    Transaction *txn_;
};

class Operators{
public:
    Operators() = delete;
    Operators(Context *context){
        transaction_manager_ = context->transaction_manager_;
        txn = context->txn_;
    }
    shared_ptr<Operators> next_node = nullptr;
    TransactionManager* transaction_manager_;
    Transaction* txn = nullptr;
    virtual vector<shared_ptr<record>> exec_op() = 0;
    void prt_op(){
        if(debug_test)
            cout << typeid(*this).name() << endl;
    }
};

int send_plan(string target_address, shared_ptr<Operators> node_plan, vector<shared_ptr<record>> &res, txn_id_t txn_id);
int get_res(const session::Table &response, vector<shared_ptr<record>>& ret_table);
// void send_plan(string target_address, shared_ptr<Operators> node_plan);
vector<shared_ptr<record>> Sql_execute(string str);
string Sql_execute_client(string str, txn_id_t &txn_id, Context* context);

class op_executor: public Operators{
public:
    op_executor(Context *context): Operators(context){};
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        return next_node->exec_op();
    }
};

class op_projection: public Operators{
public:
    op_projection(Context *context): Operators(context){};

    vector<shared_ptr<ast::Col>> cols;

    vector<shared_ptr<record>> exec_op(){
        prt_op();
        vector<shared_ptr<record>> res;
        auto ret = next_node->exec_op();
        cout << ret.size() << endl;
        for(int i = 0; i < int(ret.size()) ; i++){
            auto cur_record = ret[i];
            cur_record->pt_row();
            cout << endl;
            shared_ptr<record> new_record(new record);
            for(int j = 0; j < int(cur_record->row.size()); j++){
                auto cur_value = cur_record->row[j];
                for(int k = 0; k < int(cols.size()); k++){
                    cout << cur_value->col_name <<cols[k]->col_name << " ";
                    if(cur_value->col_name == cols[k]->col_name && (cols[k]->tab_name == "" || cols[k]->tab_name == cur_value->tab_name))
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
    op_selection(Context *context): Operators(context){};

    std::shared_ptr<ast::BinaryExpr> conds;
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        auto ret = next_node->exec_op();
        cout << "select get" << endl;
        vector<shared_ptr<record>> res;
        if(int(ret.size()) == 0){
            return res;
        }
        if(auto x = std::dynamic_pointer_cast<ast::Col>(conds->rhs)){
            int lf_index = 0;
            int rg_index = 0;
            for(int i = 0; i < int(ret[0]->row.size()); i++){
                if(ret[0]->row[i]->tab_name == conds->lhs->tab_name && ret[0]->row[i]->col_name == conds->lhs->col_name){
                    lf_index = i;
                }
                if(ret[0]->row[i]->tab_name == x->tab_name && ret[0]->row[i]->col_name == x->col_name){
                    rg_index = i;
                }
            }

            for(int i = 0; i < int(ret.size()); i++){
                if(ret[i]->row[lf_index]->str == ret[i]->row[rg_index]->str){
                    res.push_back(ret[i]);
                }
            }
        }
        else if(auto x = std::dynamic_pointer_cast<ast::IntLit>(conds->rhs)){
            int lf_index = -1;
            for(int i = 0; i < int(ret[0]->row.size()); i++){
                if(ret[0]->row[i]->tab_name == conds->lhs->tab_name && ret[0]->row[i]->col_name == conds->lhs->col_name){
                    lf_index = i;
                }
            }
            
            for(int i = 0; i < int(ret.size()); i++){
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
    op_insert(Context *context): Operators(context){
        rec = std::make_shared<record>();
    }
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        //Lock
        std::shared_lock<std::shared_mutex> l(table_name_id_map_mutex);
        auto o_id = table_name_id_map[tab_name];
        l.unlock();
        transaction_manager_->getLockManager()->LockTable(txn, LockMode::INTENTION_EXCLUSIVE, o_id);
        transaction_manager_->getLockManager()->LockPartition(txn, LockMode::EXLUCSIVE, o_id, par);
        // transaction_manager_->getLockManager()->LockRow(txn, LockMode::EXLUCSIVE, o_id, par, rec->row[0]->str);
        // 默认第一列为主键
        string key = "/store_data/" + tab_name + "/" 
            + to_string(par) + "/" + to_string(rec->row[0]->str);
        // value
        kv_store::put(key, rec);
        
        vector<shared_ptr<record>> insert_rec;
        insert_rec.push_back(rec);
        return insert_rec;
    }
};

class op_delete: public Operators{
public:
    op_delete(Context *context): Operators(context){};
    string tab_name;
    int par;
    vector<shared_ptr<record>> exec_op(){
        auto res = next_node->exec_op();

        //Lock
        std::shared_lock<std::shared_mutex> l(table_name_id_map_mutex);
        auto o_id = table_name_id_map[tab_name];
        l.unlock();
        transaction_manager_->getLockManager()->LockTable(txn, LockMode::INTENTION_EXCLUSIVE, o_id);
        transaction_manager_->getLockManager()->LockPartition(txn, LockMode::EXLUCSIVE, o_id, par);
        // for(auto x: res){
        //     transaction_manager_->getLockManager()->LockRow(txn, LockMode::EXLUCSIVE, o_id, par, x->row[0]->str);
        // }
        
        // 构造key 进行删除
        for(auto iter: res){
            auto key = "/store_data/" + tab_name + "/" + to_string(par) + "/" + to_string(iter->row[0]->str);
            kv_store::del(key);
        }
        return res;
    }
};

class op_update: public Operators{
public:
    op_update(Context *context): Operators(context){};
    string tab_name;
    int par;
    
    vector<int> set_col_index;
    vector<shared_ptr<value>> set_val;
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();
        auto res = next_node->exec_op();

        //Lock
        std::shared_lock<std::shared_mutex> l(table_name_id_map_mutex);
        auto o_id = table_name_id_map[tab_name];
        l.unlock();
        transaction_manager_->getLockManager()->LockTable(txn, LockMode::INTENTION_EXCLUSIVE, o_id);
        transaction_manager_->getLockManager()->LockPartition(txn, LockMode::EXLUCSIVE, o_id, par);
        // for(auto x: res){
        //     transaction_manager_->getLockManager()->LockRow(txn, LockMode::EXLUCSIVE, o_id, par, x->row[0]->str);
        // }
        
        // 构造key 进行更新
        for(auto iter: res){
            auto key = "/store_data/" + tab_name + "/" + to_string(par) + "/" + to_string(iter->row[0]->str);
            auto row = iter->row;
            
            cout << "check " << endl;
            for(int i = 0 ; i < int(set_col_index.size()); i++){
                int col = set_col_index[i];
                cout << col << endl;
                auto val = set_val[i];
                cout << val->str << endl;
                iter->row[col]->str = val->str;
            }
            kv_store::put(key, iter);
        }
        return res;
    }
};

class op_distribution: public Operators{
public:
    op_distribution(Context *context): Operators(context){};
    vector<shared_ptr<Operators>> nodes_plan;
    vector<string> target_address;
    string tab_name;

    vector<shared_ptr<record>> exec_op(){
        prt_op();
        vector<shared_ptr<record>> res;

        for(int i = 0 ; i < int(nodes_plan.size()); i++){
            cout << "distribution " << target_address[i] << endl;
            vector<shared_ptr<record>> ret;
            send_plan(target_address[i],nodes_plan[i], ret, txn->get_txn_id());
            res.insert(res.end(), ret.begin(), ret.end());
        }
        return res;
    }
    
};

class op_join: public Operators{
public:
    op_join(Context *context): Operators(context){};

    vector<std::shared_ptr<Operators>> tables_get;

    vector<shared_ptr<record>> exec_op(){
        // 
        vector<vector<shared_ptr<record>>> ret;
        for(int i = 0; i < int(tables_get.size()); i++){
            ret.push_back(tables_get[i]->exec_op());
        }

        auto res = ret[0];
        vector<shared_ptr<record>> tmp; 

        for(int i = 1; i < int(ret.size()); i++){
            auto tab = ret[i];
            for(auto j = 0; j < int(res.size()); j++){
                for(auto k = 0; k < int(tab.size()); k++){
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
    op_tablescan(Context *context): Operators(context){};
    string tabs;
    int par;
    
    vector<shared_ptr<record>> exec_op(){
        prt_op();

        //Lock
        cout << "lock " << endl;
        std::shared_lock<std::shared_mutex> l(table_name_id_map_mutex);
        auto o_id = table_name_id_map[tabs];
        l.unlock();
        cout << "unlock" << endl;
        transaction_manager_->getLockManager()->LockTable(txn, LockMode::INTENTION_SHARED, o_id);
        transaction_manager_->getLockManager()->LockPartition(txn, LockMode::SHARED, o_id, par);
        cout << "txn " << endl;
        vector<shared_ptr<record>> res;
        auto ret = kv_store::get_par(tabs, par, res);
        if(!ret)
            cout << "error" << endl;
        Column_info cols;
        GetColInfo(tabs, cols);
        for(auto iter: res){
            for(auto i = 0 ; i < int(iter->row.size()) ; i++){
                iter->row[i]->col_name = cols.column_name[i];
                iter->row[i]->tab_name = tabs;
            }
        }
        cout << "get table" << endl;
        return res;
    }
};
