//demo distsql engine
#include "engine.h"
#include "../session.pb.h"
#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
using namespace std;
using namespace ast;

void test_init_0(vector<shared_ptr<record>> &ret){
    for(int i = 0; i < 10; i++){
        shared_ptr<record> rec(new record);
        for(int j = 0; j < 1;j++){
            shared_ptr<value> val(new value);
            val->tab_name = "A";
            val->col_name = "id";
            val->str = i;
            rec->row.push_back(val);
        }
        rec->pt_row();
        cout << endl;
        ret.push_back(rec);
    }
}

void test_init_1(vector<shared_ptr<record>> &ret){
    for(int i = 10; i < 20; i++){
        shared_ptr<record> rec(new record);
        for(int j = 0; j < 1;j++){
            shared_ptr<value> val(new value);
            val->tab_name = "A";
            val->col_name = "id";
            val->str = i;
            rec->row.push_back(val);
        }
        rec->pt_row();
        cout << endl;
        ret.push_back(rec);
    }
}

shared_ptr<value> interp_value(const std::shared_ptr<ast::Value> &sv_val) {
    shared_ptr<value> val(new value);
    if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val)) {
        val->str = int_lit->val;
    } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val)) {
        // val.set_float(float_lit->val);
    } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val)) {
        // val.set_str(str_lit->val);
    }
    return val;
}

vector<shared_ptr<record>> get_res(session::Table &response){
        vector<shared_ptr<record>> ret_table;
        for(auto i: response.table()){
            shared_ptr<record> rec(new record);
            for(auto j: i.tuple()){
                shared_ptr<value> val(new value);
                val->str = j.value();
                rec->row.push_back(val);
            }
            ret_table.push_back(rec);
        }
        return ret_table;
    }

vector<shared_ptr<record>> send_plan(string target_address, shared_ptr<Operators> node_plan){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    session::RemotePlan request;
    session::Table response;
    brpc::Controller cntl;
    session::ChildPlan child_plan;
    string address = target_address;

    // 将我们构造好的执行计划，转成rpc通信用的message类
    // 根据我们目前的plan构造
    // 只有insert, delete, update, select，table_scan需要实现
    shared_ptr<session::ChildPlan> cur_plan(&child_plan);
    while(node_plan != nullptr){
        if (auto table_scan = dynamic_pointer_cast<op_tablescan>(node_plan)) {
            shared_ptr<session::SeqScanPlan> seq_scan_plan(new session::SeqScanPlan); 
            seq_scan_plan->set_db_name(DB_NAME);
            seq_scan_plan->set_tab_name(table_scan->tabs);
            seq_scan_plan->set_par_id(table_scan->par);
            cur_plan->set_allocated_seq_scan_plan(seq_scan_plan.get());
            cur_plan.reset(seq_scan_plan->add_child());
        }
        else if (auto insert_message = dynamic_pointer_cast<op_insert>(node_plan)) {
            // 实现insert到message构造
            shared_ptr<session::InsertPlan> insert_plan(new session::InsertPlan);
            insert_plan->set_db_name(DB_NAME);
            insert_plan->set_tab_name(insert_message->tab_name);
            insert_plan->set_par_id(insert_message->par);

            for (auto col : insert_message->rec->row) {
                insert_plan->add_col_name(col->col_name);
            }

            for (auto val : insert_message->rec->row) {
                    insert_plan->add_val(val->str);
            }

            cur_plan->set_allocated_insert_plan(insert_plan.get());
            cur_plan.reset(insert_plan->add_child());
        }
        else if(auto update_message = dynamic_pointer_cast<op_update>(node_plan)){
            // 实现update到message构造
            shared_ptr<session::UpdatePlan> update_plan(new session::UpdatePlan);
            update_plan->set_db_name(DB_NAME);
            update_plan->set_tab_name(update_message->tab_name);
            update_plan->set_par_id(update_message->par);

            for(auto col : update_message->set_col_index){
                update_plan->add_set_col_index(col);
            }

            for(auto col : update_message->set_val){
                update_plan->add_set_col_index(col->str);
            }

            cur_plan->set_allocated_update_plan(update_plan.get());
            cur_plan.reset(update_plan->add_child());
        }
        else if(auto delete_message = dynamic_pointer_cast<op_delete>(node_plan)){
            // 实现delete到message构造
            shared_ptr<session::DeletePlan> delete_plan(new session::DeletePlan);
            delete_plan->set_db_name(DB_NAME);
            delete_plan->set_tab_name(delete_message->tab_name);
            delete_plan->set_par_id(delete_message->par);

            cur_plan->set_allocated_delete_plan(delete_plan.get());
            cur_plan.reset(delete_plan->add_child());
        }
        else if(auto select_message = dynamic_pointer_cast<op_selection>(node_plan)){
            // 实现select到message构造
            shared_ptr<session::FilterPlan> select_plan(new session::FilterPlan);
            shared_ptr<session::BinaryMessage> cond_msg(new session::BinaryMessage);

            shared_ptr<session::Col> col_msg(new session::Col);

            // Set the values for the Col message
            col_msg->set_tab_name(select_message->conds->lhs->tab_name);
            col_msg->set_col_name(select_message->conds->lhs->col_name);

            // Set the values for the BinaryMessage message
            cond_msg->set_op(select_message->conds->op);

            shared_ptr<session::Expr> right(new session::Expr);
            if(auto x = dynamic_pointer_cast<Col>(select_message->conds->rhs)){
                shared_ptr<session::Col> col_right_msg(new session::Col);
                col_right_msg->set_tab_name(x->tab_name);
                col_right_msg->set_col_name(x->col_name);
                right->set_allocated_col(col_right_msg.get());
            }
            else if(auto x = dynamic_pointer_cast<ast::Value>(select_message->conds->rhs)){
                auto val = interp_value(x);
                shared_ptr<session::Value> col_right_msg(new session::Value);
                col_right_msg->set_value(val->str);
                right->set_allocated_val(col_right_msg.get());
            }
            cur_plan->set_allocated_filter_plan(select_plan.get());
            cur_plan.reset(select_plan->add_child());
        }
    }

    options.timeout_ms = 100;
    options.max_retry = 3;

    cout << address << endl;
    if (channel.Init(address.c_str(), &options) != 0){
        LOG(ERROR) << "Fail to initialize channel";
    }
    session::Session_Service_Stub stub(&channel);
    int log_id = 0;
    /* code */
    request.set_allocated_child(&child_plan);
    cntl.set_log_id(log_id ++);
            
    stub.SendRemotePlan(&cntl, &request, &response, NULL);
            
    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    LOG(INFO) << "Client is going to quit";
    
    return get_res(response);
}

void parser_sql(string str){
    YY_BUFFER_STATE buf = yy_scan_string(str.c_str());
}

int GetTableInfo(string tab_name, Column_info &ret){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    if (channel.Init(FLAGS_META_SERVER_ADDR.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }
    meta_service::MetaService_Stub stub(&channel);
    brpc::Controller cntl;
    meta_service::GetTableInfoRequest request;
    meta_service::GetTableInfoResponse response;
    
    request.set_tab_name(DB_NAME);
    request.set_tab_name(tab_name);

    stub.GetTableInfo(&cntl, &request, &response, NULL);

    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    LOG(INFO) << "Client is going to quit";

    for(int i = 0; i < response.col_name_size(); i++){
        ret.column_name.push_back(response.col_name()[i]);
        ret.column_type.push_back(ColType(response.col_type()[i]));
    }

    return 1;
}

map<int,string> GetTableIp(string tab_name, string partition_key_name, string min_range = "0", string max_range = "1000000"){
    map<int,string> res;// 分区，ip
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    if (channel.Init(FLAGS_META_SERVER_ADDR.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return res;
    }
    meta_service::MetaService_Stub stub(&channel);
    brpc::Controller cntl;
    meta_service::PartitionLocationRequest request;
    meta_service::PartitionLocationResponse response;
    meta_service::PartitionLocationRequest_StringRange range;
    
    request.set_tab_name(DB_NAME);
    request.set_tab_name(tab_name);
    request.set_partition_key_name(partition_key_name);
    range.set_min_range(min_range);
    range.set_max_range(max_range);
    

    stub.GetPartitionLocation(&cntl, &request, &response, NULL);

    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    LOG(INFO) << "Client is going to quit";

    auto ret = response.pid_partition_location();
    for(auto iter = ret.begin(); iter != ret.end(); iter++){
        res[iter->first] = iter->second.ip_addr() + ":" +to_string(iter->second.port());
    }
    return res;
}

int create_table(string tab_name, std::vector<ColDef> col_defs){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 100;
    options.max_retry = 3;

    if (channel.Init(FLAGS_META_SERVER_ADDR.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }
    meta_service::MetaService_Stub stub(&channel);
    brpc::Controller cntl;
    meta_service::CreateTableRequest request;
    meta_service::CreateTableResponse response;
    
    request.set_tab_name(DB_NAME);
    request.set_tab_name(tab_name);
    for(auto col: col_defs){
        request.add_col_name(col.col_name);
        request.add_col_type(col.type_len->type);
    }

    stub.CreateTable(&cntl, &request, &response, NULL);

    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    LOG(INFO) << "Client is going to quit";

    
    return response.success();
}

void build_insert_plan(shared_ptr<ast::InsertStmt> insert_tree, std::shared_ptr<op_executor> exec_plan){
    // 构造insert算子
    shared_ptr<op_insert> insert_plan(new op_insert);
    insert_plan->tab_name = insert_tree->tab_name;
    insert_plan->rec.reset(new record);
    auto iter = insert_plan->rec;
    Column_info col_info;
    GetTableInfo(insert_tree->tab_name, col_info);
    for(int i = 0; i < insert_tree->vals.size(); i++){
        auto in_val = interp_value(insert_tree->vals[i]);
        in_val->tab_name = insert_tree->tab_name;
        in_val->col_name = col_info.column_name[i];
        iter->row.push_back(in_val);
    }
    // 默认第一列为主键
    auto par_ips = GetTableIp(insert_plan->tab_name, col_info.column_name[0], to_string(iter->row[0]->str), to_string(iter->row[0]->str));
    // insert只可能向某一特定位置插入，最后只可能返回一组ip
    auto ip_address = par_ips.begin()->second;
    insert_plan->par = par_ips.begin()->first;

    // 构造一个distribution 发过去
    shared_ptr<op_distribution> distribution_plan(new op_distribution);
    exec_plan->next_node = distribution_plan;
    distribution_plan->nodes_plan.push_back(insert_plan);
    distribution_plan->target_address.push_back(ip_address);
}

void build_delete_plan(shared_ptr<ast::DeleteStmt> delete_tree, std::shared_ptr<op_executor> exec_plan){
    shared_ptr<Operators> cur_node = exec_plan;
    // 针对不同ip构造不同delete_plan,发个不同节点
    Column_info col_info;
    GetTableInfo(delete_tree->tab_name, col_info);
    auto tab_name = delete_tree->tab_name;
    // 默认第一列为主键
    auto par_ips = GetTableIp(tab_name, col_info.column_name[0], to_string(0), to_string(10000000));
    shared_ptr<op_distribution> distribution_plan(new op_distribution);
    for(auto iter: par_ips){
        shared_ptr<op_delete> delete_plan(new op_delete);
        delete_plan->tab_name = tab_name;
        delete_plan->par = iter.first;
        cur_node->next_node = delete_plan;
        cur_node = cur_node->next_node;
        // 筛选
        for(int i = 0; i < delete_tree->conds.size(); i++){
            if(delete_tree->conds.size()){
                shared_ptr<op_selection> selection_plan(new op_selection);
                selection_plan->conds = delete_tree->conds[i];
                
                cur_node->next_node = selection_plan;
                cur_node = cur_node->next_node;
            }
        }
        // 全表扫描
        shared_ptr<op_tablescan> scan_plan(new op_tablescan);
        scan_plan->tabs = delete_tree->tab_name;
        scan_plan->par = iter.first;
        cur_node->next_node = scan_plan;
        distribution_plan->nodes_plan.push_back(delete_plan);
        distribution_plan->target_address.push_back(iter.second);
    }
}

void build_update_plan(shared_ptr<ast::UpdateStmt> update_tree, std::shared_ptr<op_executor> exec_plan){
    shared_ptr<Operators> cur_node = exec_plan;
    // 针对不同ip构造不同delete_plan,发个不同节点
    Column_info col_info;
    GetTableInfo(update_tree->tab_name, col_info);
    auto tab_name = update_tree->tab_name;
    // 默认第一列为主键
    auto par_ips = GetTableIp(tab_name, col_info.column_name[0]);
    shared_ptr<op_distribution> distribution_plan(new op_distribution);
    for(auto iter: par_ips){
        shared_ptr<op_update> update_plan(new op_update);
        cur_node->next_node = update_plan;
        cur_node = cur_node->next_node;
        // 构造
        update_plan->par = iter.first;
        update_plan->tab_name = tab_name;
        for(auto set_col: update_tree->set_clauses){
            for(int i = 0; i < col_info.column_name.size(); i++){
                if(set_col->col_name == col_info.column_name[i]){
                    update_plan->set_col_index.push_back(i);
                    update_plan->set_val.push_back(interp_value(set_col->val));
                }
            }
        }
        // 筛选
        for(int i = 0; i < update_tree->conds.size(); i++){
            if(update_tree->conds.size()){
                shared_ptr<op_selection> selection_plan(new op_selection);
                selection_plan->conds = update_tree->conds[i];
                
                cur_node->next_node = selection_plan;
                cur_node = cur_node->next_node;
            }
        }
        // 全表扫描
        shared_ptr<op_tablescan> scan_plan(new op_tablescan);
        scan_plan->tabs = update_tree->tab_name;
        scan_plan->par = iter.first;
        cur_node->next_node = scan_plan;
        distribution_plan->nodes_plan.push_back(update_plan);
        distribution_plan->target_address.push_back(iter.second);
    }
}

void build_select_plan(std::shared_ptr<ast::SelectStmt> select_tree, std::shared_ptr<op_executor> exec_plan) {
    std::shared_ptr<Operators> cur_node = exec_plan;

    // 处理投影操作
    if (select_tree->cols.size()) {
        std::shared_ptr<op_projection> projection_plan(new op_projection);
        projection_plan->cols = select_tree->cols;
        cur_node->next_node = projection_plan;
        cur_node = cur_node->next_node;
    }

    // 处理选择操作
    for (auto& cond : select_tree->conds) {
        if (!cond) continue; // 忽略空条件
        std::shared_ptr<op_selection> selection_plan(new op_selection);
        selection_plan->conds = cond;
        cur_node->next_node = selection_plan;
        cur_node = cur_node->next_node;
    }

    // 处理连接操作
    if (select_tree->tabs.size() > 1) {
        std::shared_ptr<op_join> join_plan(new op_join);
        for (auto& tab : select_tree->tabs) {
            Column_info col_info;
            GetTableInfo(tab, col_info);
            auto par_ips = GetTableIp(tab, col_info.column_name[0]);
            // 构造分布查询
            if(par_ips.empty()){
                LOG(ERROR) << "Fail to get partition location for table " << tab;
                return;
            }
            // 构造分布查询
            shared_ptr<op_distribution> dist_plan(new op_distribution);
            for(auto iter: par_ips){
                std::shared_ptr<op_tablescan> remote_scan_plan(new op_tablescan);
                remote_scan_plan->tabs = tab;
                remote_scan_plan->par = iter.first;
                dist_plan->nodes_plan.push_back(remote_scan_plan);
                dist_plan->target_address.push_back(iter.second);
            }
            join_plan->tables_get.push_back(dist_plan);
        }
        cur_node->next_node = join_plan;
        cur_node = cur_node->next_node;
    } else if (select_tree->tabs.size() == 1){
        Column_info col_info;
        GetTableInfo(select_tree->tabs[0], col_info);
        auto par_ips = GetTableIp(select_tree->tabs[0], col_info.column_name[0]);
        // 构造分布查询
        if(par_ips.empty()){
            LOG(ERROR) << "Fail to get partition location for table " << select_tree->tabs[0];
            return;
        }
            // 构造分布查询
        shared_ptr<op_distribution> dist_plan(new op_distribution);
        for(auto iter: par_ips){
            std::shared_ptr<op_tablescan> remote_scan_plan(new op_tablescan);
            remote_scan_plan->tabs = select_tree->tabs[0];
            remote_scan_plan->par = iter.first;
            dist_plan->nodes_plan.push_back(remote_scan_plan);
            dist_plan->target_address.push_back(iter.second);
        }
        cur_node->next_node = dist_plan;
        cur_node = cur_node;
    }
    else{
        LOG(ERROR) << "No table " << select_tree->tabs[0];
    }
}

void prt_plan(shared_ptr<Operators> exec_plan){
    auto tmp = exec_plan;
    while(tmp != nullptr){
        cout << typeid(*tmp).name() << endl;
        tmp = tmp->next_node;
    }
    cout << "over_plan" << endl;
}


vector<shared_ptr<record>> Sql_execute(string str){
    parser_sql(str);
    vector<shared_ptr<record>> res;
    if (yyparse() == 0) {
        ast::TreePrinter::print(ast::parse_tree);
        if (ast::parse_tree != nullptr) {
            auto root = ast::parse_tree;
            if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(root)) {
                shared_ptr<op_executor> exec_plan(new op_executor);
                build_select_plan(x, exec_plan);
                res = exec_plan->exec_op();
                // prt_plan(exec_plan);
            } else if(auto x = std::dynamic_pointer_cast<ast::CreateTable>(ast::parse_tree)){
                std::vector<ColDef> col_defs;
                for (auto &field : x->fields) {
                    if (auto parser_col_def = std::dynamic_pointer_cast<ast::ColDef>(field)) {
                        ColDef col_def;
                        col_def.col_name = parser_col_def->col_name;
                        col_def.type_len.reset(new TypeLen(parser_col_def->type_len->type,parser_col_def->type_len->len));
                        col_defs.push_back(col_def);
                    } else {
                        cout << "Unexpected field type" << endl;
                    }
                }
                if(create_table(x->tab_name, col_defs)){
                    cout << "create table " << x->tab_name << endl;
                } else{
                    cout << "fail create table" << endl;
                }
            } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(root)) {
            // insert;
                // 根据table 信息 转成record类型
                // 找到对应的表ip进行插入
                shared_ptr<op_executor> exec_plan(new op_executor);
                build_insert_plan(x, exec_plan);
                res = exec_plan->exec_op();
            } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(root)) {
                // delete;
                shared_ptr<op_executor> exec_plan(new op_executor);
                build_delete_plan(x, exec_plan);
                res = exec_plan->exec_op();
            } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(root)) {
                // update;
                // std::vector<Condition> conds = interp_where_clause(x->conds);
                // std::vector<SetClause> set_clauses;
                // for (auto &sv_set_clause : x->set_clauses) {
                //     SetClause set_clause = {.lhs = {.tab_name = "", .col_name = sv_set_clause->col_name},
                //                             .rhs = interp_sv_value(sv_set_clause->val)};
                //     set_clauses.push_back(set_clause);
                // }
                // SetTransaction(txn_id, context);
                // ql_manager_->update_set(x->tab_name, set_clauses, conds, context);
        }
    }
    return res;
}