//demo distsql engine
#include "engine.h"
#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
using namespace std;

std::map<string, string> nodes_name{{"0.0.0.0:8005", "server1"},{"0.0.0.0:8006", "server2"}};

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

int get_res(const session::Table &response, vector<shared_ptr<record>>& ret_table){
    for(auto i: response.table()){
        shared_ptr<record> rec = make_shared<record>();
        for(auto j: i.tuple()){
            shared_ptr<value> val = make_shared<value>();
            val->str = j.value();
            val->col_name = j.col_name();
            val->tab_name = j.tab_name();
            rec->row.push_back(val);
        }
        ret_table.push_back(rec);
    }
    return ret_table.size();
}

int send_plan(string target_address, shared_ptr<Operators> node_plan, vector<shared_ptr<record>>& res, txn_id_t txn_id){
    brpc::Channel channel;
    brpc::ChannelOptions options;
    session::RemotePlan request;
    session::Table response;
    brpc::Controller cntl;
    session::ChildPlan* child_plan = new session::ChildPlan;
    string address = target_address;

    // 将我们构造好的执行计划，转成rpc通信用的message类
    // 根据我们目前的plan构造
    // 只有insert, delete, update, select，table_scan需要实现
    cout << "send_plan: " << endl;
    auto cur_plan = child_plan;
    while(node_plan != nullptr){
        if (auto table_scan = dynamic_pointer_cast<op_tablescan>(node_plan)) {
            cout << "table_scan " << endl;
            auto seq_scan_plan = new session::SeqScanPlan;

            seq_scan_plan->set_db_name(DB_NAME);
            seq_scan_plan->set_tab_name(table_scan->tabs);
            seq_scan_plan->set_par_id(table_scan->par);

            cur_plan->set_allocated_seq_scan_plan(seq_scan_plan);
            cur_plan = seq_scan_plan->add_child();
        }
        else if (auto insert_message = dynamic_pointer_cast<op_insert>(node_plan)) {
            // 实现insert到message构造
            cout << "insert_plan " << endl;
            auto insert_plan = new session::InsertPlan;
            insert_plan->set_db_name(DB_NAME);
            insert_plan->set_tab_name(insert_message->tab_name);
            insert_plan->set_par_id(insert_message->par);
            

            for (auto val : insert_message->rec->row) {
                insert_plan->add_col_name(val->col_name);
                insert_plan->add_val(val->str);
            }
            cur_plan->set_allocated_insert_plan(insert_plan);
            cur_plan = insert_plan->add_child();
        }
        else if(auto update_message = dynamic_pointer_cast<op_update>(node_plan)){
            cout << "update_plan " << endl;
            // 实现update到message构造
            auto update_plan = new session::UpdatePlan;
            update_plan->set_db_name(DB_NAME);
            update_plan->set_tab_name(update_message->tab_name);
            update_plan->set_par_id(update_message->par);

            for(auto col : update_message->set_col_index){
                update_plan->add_set_col_index(col);
            }

            for(auto col : update_message->set_val){
                update_plan->add_set_val(col->str);
            }

            cur_plan->set_allocated_update_plan(update_plan);
            cur_plan = update_plan->add_child();
        }
        else if(auto delete_message = dynamic_pointer_cast<op_delete>(node_plan)){
            // 实现delete到message构造
            cout << "delete_plan " << endl;
            auto delete_plan = new session::DeletePlan;

            delete_plan->set_db_name(DB_NAME);
            delete_plan->set_tab_name(delete_message->tab_name);
            delete_plan->set_par_id(delete_message->par);

            cur_plan->set_allocated_delete_plan(delete_plan);
            cur_plan = delete_plan->add_child();
        }
        else if(auto select_message = dynamic_pointer_cast<op_selection>(node_plan)){
            // 实现select到message构造
            cout << "select_plan " << endl;
            auto select_plan = new session::FilterPlan;
            auto cond_msg = new session::BinaryMessage;

            auto col_msg = new session::Col;

            // Set the values for the Col message
            col_msg->set_tab_name(select_message->conds->lhs->tab_name);
            col_msg->set_col_name(select_message->conds->lhs->col_name);
            cond_msg->set_allocated_col(col_msg);
            
            cond_msg->set_op(select_message->conds->op);
            
            auto right = new session::Expr;
            if(auto x = dynamic_pointer_cast<ast::Col>(select_message->conds->rhs)){
                auto col_right_msg = new session::Col;
                col_right_msg->set_tab_name(x->tab_name);
                col_right_msg->set_col_name(x->col_name);
                right->set_allocated_col(col_right_msg);
            }
            else if(auto x = dynamic_pointer_cast<ast::Value>(select_message->conds->rhs)){
                auto val = interp_value(x);
                auto col_right_msg = new session::Value;
                col_right_msg->set_value(val->str);
                right->set_allocated_val(col_right_msg);
            }
            cond_msg->set_allocated_exp(right);

            select_plan->set_allocated_conds(cond_msg);
            cur_plan->set_allocated_filter_plan(select_plan);
            cur_plan = select_plan->add_child();
        }

        node_plan = node_plan->next_node;
    }

    options.timeout_ms = 0xfffffff;
    options.max_retry = 3;

    cout << address << endl;
    if (channel.Init(address.c_str(), &options) != 0){
        LOG(ERROR) << "Fail to initialize channel";
    }
    session::Session_Service_Stub stub(&channel);
    int log_id = 0;
    /* code */
    request.set_allocated_child(child_plan);
    request.set_txn_id(txn_id);
    cntl.set_log_id(log_id ++);
    stub.SendRemotePlan(&cntl, &request, &response, NULL);
    if(response.abort())
        throw TransactionAbortException (txn_id, AbortReason::PARTICIPANT_RETURN_ABORT);

    if(cntl.Failed()) {
        LOG(WARNING) << cntl.ErrorText();
    }
    LOG(INFO) << "Client is going to quit";
    
    get_res(response, res);
    return res.size();
}

void parser_sql(string str){
    // YY_BUFFER_STATE buf = yy_scan_string(str.c_str());
    yy_scan_string(str.c_str());
}

bool create_table(shared_ptr<ast::CreateTable> create_table){
    auto tab_name = create_table->tab_name;
    shared_ptr<Column_info> cols_info(new Column_info);
    vector<int> par_vals;
    for (auto &field : create_table->fields) {
        if (auto parser_col_def = std::dynamic_pointer_cast<ast::ColDef>(field)) {
            cols_info->column_name.push_back(parser_col_def->col_name);
            cols_info->column_type.push_back(ColType(int(parser_col_def->type_len->type)));
        } else {
            cout << "Unexpected field type" << endl;
        }
    }
    par_vals.push_back(par_min);
    if(create_table->par_opt != nullptr)
        for (auto &par_val : create_table->par_opt->parts){
            par_vals.push_back(par_val->val);
        }
    par_vals.push_back(par_max);
    auto res = create_par_table(tab_name, cols_info, par_vals);
    return res;
}


void build_insert_plan(shared_ptr<ast::InsertStmt> insert_tree, std::shared_ptr<op_executor> exec_plan, Context* context){
    // 构造insert算子
    shared_ptr<op_insert> insert_plan(new op_insert(context));
    insert_plan->tab_name = insert_tree->tab_name;
    insert_plan->rec.reset(new record);
    auto iter = insert_plan->rec;
    Column_info col_info;
    GetColInfo(insert_tree->tab_name, col_info);
    for(int i = 0; i < int(insert_tree->vals.size()); i++){
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
    for(auto x: par_ips){
        context->txn_->add_distributed_node_set(x.second);
    }
    // 构造一个distribution 发过去
    shared_ptr<op_distribution> distribution_plan(new op_distribution(context));
    exec_plan->next_node = distribution_plan;
    distribution_plan->nodes_plan.push_back(insert_plan);
    distribution_plan->target_address.push_back(ip_address);
}

void build_delete_plan(shared_ptr<ast::DeleteStmt> delete_tree, std::shared_ptr<op_executor> exec_plan, Context* context){
    shared_ptr<Operators> cur_node = exec_plan;
    // 针对不同ip构造不同delete_plan,发个不同节点
    Column_info col_info;
    GetColInfo(delete_tree->tab_name, col_info);
    auto tab_name = delete_tree->tab_name;
    // 默认第一列为主键
    auto par_ips = GetTableIp(tab_name, col_info.column_name[0], to_string(0), to_string(10000000));
    shared_ptr<op_distribution> distribution_plan(new op_distribution(context));
    cur_node->next_node = distribution_plan;
    for(auto iter: par_ips){
        context->txn_->add_distributed_node_set(iter.second);

        shared_ptr<op_delete> delete_plan(new op_delete(context));
        delete_plan->tab_name = tab_name;
        delete_plan->par = iter.first;
        cur_node = delete_plan;
        // 筛选
        for(int i = 0; i < int(delete_tree->conds.size()); i++){
            if(delete_tree->conds.size()){
                shared_ptr<op_selection> selection_plan(new op_selection(context));
                selection_plan->conds = delete_tree->conds[i];
                
                cur_node->next_node = selection_plan;
                cur_node = cur_node->next_node;
            }
        }
        // 全表扫描
        shared_ptr<op_tablescan> scan_plan(new op_tablescan(context));
        scan_plan->tabs = delete_tree->tab_name;
        scan_plan->par = iter.first;
        cur_node->next_node = scan_plan;
        distribution_plan->nodes_plan.push_back(delete_plan);
        distribution_plan->target_address.push_back(iter.second);
    }
}

void build_update_plan(shared_ptr<ast::UpdateStmt> update_tree, std::shared_ptr<op_executor> exec_plan, Context* context){
    shared_ptr<Operators> cur_node = exec_plan;
    // 针对不同ip构造不同delete_plan,发个不同节点
    Column_info col_info;
    GetColInfo(update_tree->tab_name, col_info);
    auto tab_name = update_tree->tab_name;
    // 默认第一列为主键
    auto par_ips = GetTableIp(tab_name, col_info.column_name[0]);
    shared_ptr<op_distribution> distribution_plan(new op_distribution(context));
    cur_node->next_node = distribution_plan;
    cur_node = distribution_plan;
    for(auto iter: par_ips){
        context->txn_->add_distributed_node_set(iter.second);
        
        shared_ptr<op_update> update_plan(new op_update(context));
        cur_node = update_plan;
        // 构造
        update_plan->par = iter.first;
        update_plan->tab_name = tab_name;
        for(auto set_col: update_tree->set_clauses){
            // cout << set_col->col_name << endl;
            for(int i = 0; i < int(col_info.column_name.size()); i++){ 
                // cout << col_info.column_name[i] << endl;
                if(set_col->col_name == col_info.column_name[i]){
                    update_plan->set_col_index.push_back(i);
                    update_plan->set_val.push_back(interp_value(set_col->val));
                }
            }
        }
        // 筛选
        for(int i = 0; i < int(update_tree->conds.size()); i++){
            if(update_tree->conds.size()){
                shared_ptr<op_selection> selection_plan(new op_selection(context));
                selection_plan->conds = update_tree->conds[i];
                
                cur_node->next_node = selection_plan;
                cur_node = cur_node->next_node;
            }
        }
        // 全表扫描
        shared_ptr<op_tablescan> scan_plan(new op_tablescan(context));
        scan_plan->tabs = update_tree->tab_name;
        scan_plan->par = iter.first;
        cur_node->next_node = scan_plan;
        distribution_plan->nodes_plan.push_back(update_plan);
        distribution_plan->target_address.push_back(iter.second);
    }
}

void build_select_plan(std::shared_ptr<ast::SelectStmt> select_tree, std::shared_ptr<op_executor> exec_plan, Context* context) {
    std::shared_ptr<Operators> cur_node = exec_plan;

    // 处理投影操作
    if (select_tree->cols.size()) {
        std::shared_ptr<op_projection> projection_plan(new op_projection(context));
        for(auto iter: select_tree->cols){
            shared_ptr<ast::Col> col(new ast::Col(iter->tab_name,iter->col_name));
            projection_plan->cols.push_back(col);
        }
        cur_node->next_node = projection_plan;
        cur_node = cur_node->next_node;
    }

    // 处理选择操作
    for (auto& cond : select_tree->conds) {
        if (!cond) continue; // 忽略空条件
        std::shared_ptr<op_selection> selection_plan(new op_selection(context));
        selection_plan->conds = cond;
        cur_node->next_node = selection_plan;
        cur_node = cur_node->next_node;
    }

    // 处理连接操作
    if (select_tree->tabs.size() > 1) {
        std::shared_ptr<op_join> join_plan(new op_join(context));
        for (auto& tab : select_tree->tabs) {
            Column_info col_info;
            GetColInfo(tab, col_info);
            auto par_ips = GetTableIp(tab, col_info.column_name[0]);
            // 构造分布查询
            if(par_ips.empty()){
                LOG(ERROR) << "Fail to get partition location for table " << tab;
                return;
            }
            // 构造分布查询
            shared_ptr<op_distribution> dist_plan(new op_distribution(context));
            for(auto iter: par_ips){
                context->txn_->add_distributed_node_set(iter.second);
                std::shared_ptr<op_tablescan> remote_scan_plan(new op_tablescan(context));
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
        GetColInfo(select_tree->tabs[0], col_info);
        auto par_ips = GetTableIp(select_tree->tabs[0], col_info.column_name[0]);
        // 构造分布查询
        if(par_ips.empty()){
            LOG(ERROR) << "Fail to get partition location for table " << select_tree->tabs[0];
            return;
        }
            // 构造分布查询
        shared_ptr<op_distribution> dist_plan(new op_distribution(context));
        for(auto iter: par_ips){
            context->txn_->add_distributed_node_set(iter.second);
            std::shared_ptr<op_tablescan> remote_scan_plan(new op_tablescan(context));
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

void prt_plan(shared_ptr<Operators> exec_plan, vector<vector<string>> &res_txt, string offset, string node){
    auto tmp = exec_plan;
    vector<string> row;
    string op = offset + typeid(*tmp).name();
    cout << op << endl;
    row.push_back(op);
    row.push_back(node);
    res_txt.push_back(row);
    offset += "  ";
    if(auto x  = std::dynamic_pointer_cast<op_distribution>(tmp)){
        int nums = x->nodes_plan.size();
        for(int i = 0; i < nums; i++){
            prt_plan(x->nodes_plan[i], res_txt, offset, nodes_name[x->target_address[i]]);
        }
    } else if(auto x = std::dynamic_pointer_cast<op_join>(tmp)){
        int nums = x->tables_get.size();
        for(int i = 0; i < nums; i++){
            prt_plan(x->tables_get[i], res_txt, offset, node);
        }
    } else if(auto x = tmp->next_node){
        if(x == nullptr){
            return;
        }
        prt_plan(x, res_txt, offset, node);
    } else {
        return;
    }
}

bool form_stream(stringstream &str_out, int val_len, int col_nums){
    for(int i = 0; i < col_nums; i++){
        str_out << '+';
        for(int j = 0; j < val_len -1; j++){
            str_out << '-';
        }
    }
    str_out << '+' << endl;
    return 1;
}

bool table_to_string(vector<shared_ptr<record>> &table, string &str){
    stringstream str_out;
    int val_len = 20;
    int col_nums = table[0]->row.size();

    form_stream(str_out, val_len, col_nums);

    auto cols = table[0]->row;
    for(int i = 0; i < col_nums; i++){
        string tmp = "| " + cols[i]->col_name;
        str_out << setiosflags(ios::left) << setw(val_len) << tmp;
    }
    str_out << '|' << endl;

    form_stream(str_out, val_len, col_nums);

    for(auto row: table){
        for(int i = 0; i < col_nums; i++){
            string tmp = "| " + to_string(row->row[i]->str);
            str_out << setiosflags(ios::left) << setw(val_len) << tmp;
        }
        str_out << '|' << endl;
    }

    form_stream(str_out, val_len, col_nums);

    string out;
    while(getline(str_out, out)){
        str.append(out + "\n");
    }
    return 1;
}

bool txt_to_string(vector<vector<string>> &res_txt, string &str){
    stringstream str_out;
    int val_len = 25;
    int col_nums = res_txt[0].size();
    form_stream(str_out, val_len, col_nums);

    auto cols = res_txt[0];
    for(int i = 0; i < col_nums; i++){
        string tmp = "| " + cols[i];
        str_out << setiosflags(ios::left) << setw(val_len) << tmp;
    }
    str_out << '|' << endl;

    form_stream(str_out, val_len, col_nums);

    for(int i = 1; i < int(res_txt.size()); i++){
        auto row = res_txt[i];
        for(auto col: row){
            string tmp = "| " + col;
            str_out << setiosflags(ios::left) << setw(val_len) << tmp;
        }
        str_out << '|' << endl;
    }

    form_stream(str_out, val_len, col_nums);

    string out;
    while(getline(str_out, out)){
        str.append(out + "\n");
    }
    return 1;
}

bool showTables(vector<vector<string>> &res_txt){
    vector<string> col_name;
    col_name.push_back("Tables");
    res_txt.push_back(col_name);

    GetTables(res_txt);
    return true;
}

bool showPartitions(vector<vector<string>> &res_txt){
    vector<string> col_name;
    col_name.push_back("Table_Name");
    col_name.push_back("Partition_Key");
    col_name.push_back("Partition_Num");
    col_name.push_back("Ip_address");
    col_name.push_back("Range");
    res_txt.push_back(col_name);

    GetPartitions(res_txt);

    return true;
}

bool showPlan(std::shared_ptr<ast::Explain> explain_tree, vector<vector<string>> &res_txt, Context* context){
    auto stmt = explain_tree->stmt;
    shared_ptr<op_executor> exec_plan(new op_executor(context));
    if (ast::parse_tree != nullptr) {
        auto root = stmt;
        if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(root)) {
            build_select_plan(x, exec_plan, context);
        } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(root)) {
            build_insert_plan(x, exec_plan, context);
        } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(root)) {
            build_delete_plan(x, exec_plan, context);
        } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(root)) {
            build_update_plan(x, exec_plan, context);
        }
    }
    vector<string> col_name;
    col_name.push_back("Operators");
    col_name.push_back("Node"); 
    res_txt.push_back(col_name);
    prt_plan(exec_plan, res_txt, "", "server1");
    
    return true;
}

string Sql_execute_client(string str, txn_id_t &txn_id, Context* context){
    parser_sql(str);
    vector<shared_ptr<record>> res;
    vector<vector<string>> res_txt;
    string ok_txt = "";
    if (yyparse() == 0) {
        ast::TreePrinter::print(ast::parse_tree);
        if (ast::parse_tree != nullptr) {
            auto root = ast::parse_tree;
            if (auto x = std::dynamic_pointer_cast<ast::ShowTables>(root)){
                showTables(res_txt);
            } else if (auto x = std::dynamic_pointer_cast<ast::ShowPartitions>(root)) {
                showPartitions(res_txt);
            } else if (auto x = std::dynamic_pointer_cast<ast::Explain>(root)) {
                showPlan(x, res_txt, context);
            }else if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(root)) {
                // 开始事务
                shared_ptr<op_executor> exec_plan(new op_executor(context));
                if(!txn_id){
                    exec_plan->transaction_manager_->Begin(exec_plan->txn);    
                } else{
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                }
                context->txn_ = exec_plan->txn;
                cout << "txn = " << txn_id << endl;
                build_select_plan(x, exec_plan, context);
                res = exec_plan->exec_op();
                // 结束事务
                // prt_plan(exec_plan);
                if(!txn_id){
                    exec_plan->transaction_manager_->Commit(exec_plan->txn);
                } else{
                    txn_id = exec_plan->txn->get_txn_id();
                }
            } else if(auto x = std::dynamic_pointer_cast<ast::CreateTable>(ast::parse_tree)){
                if(create_table(x)){
                    ok_txt = "OK";
                    cout << "create table " << x->tab_name << endl;
                } else{
                    ok_txt = "error";
                    cout << "fail create table" << endl;
                }
            } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(root)) {
            // insert;
                // 根据table 信息 转成record类型
                // 找到对应的表ip进行插入
                shared_ptr<op_executor> exec_plan(new op_executor(context));
                if(!txn_id){
                    exec_plan->transaction_manager_->Begin(exec_plan->txn);
                } else{
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                }
                context->txn_ = exec_plan->txn;
                build_insert_plan(x, exec_plan, context);
                res = exec_plan->exec_op();
                ok_txt = "OK";
                if(!txn_id){
                    exec_plan->transaction_manager_->Commit(exec_plan->txn);
                } else{
                    txn_id = exec_plan->txn->get_txn_id();
                }
            } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(root)) {
                // delete;
                shared_ptr<op_executor> exec_plan(new op_executor(context));
                if(!txn_id){
                    exec_plan->transaction_manager_->Begin(exec_plan->txn);
                } else{
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                }
                context->txn_ = exec_plan->txn;
                build_delete_plan(x, exec_plan, context);
                res = exec_plan->exec_op();
                ok_txt = "OK";
                if(!txn_id){
                    exec_plan->transaction_manager_->Commit(exec_plan->txn);
                } else{
                    txn_id = exec_plan->txn->get_txn_id();
                }
            } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(root)) {
                shared_ptr<op_executor> exec_plan(new op_executor(context));
                if(!txn_id){
                    exec_plan->transaction_manager_->Begin(exec_plan->txn);
                } else{
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                }
                context->txn_ = exec_plan->txn;
                build_update_plan(x, exec_plan, context);
                res = exec_plan->exec_op();
                ok_txt = "OK";
                if(!txn_id){
                    exec_plan->transaction_manager_->Commit(exec_plan->txn);
                } else{
                    txn_id = exec_plan->txn->get_txn_id();
                }
            } else if (auto x = std::dynamic_pointer_cast<ast::TxnBegin>(root)) {
                // begin;
                shared_ptr<op_executor> exec_plan(new op_executor(context));
                exec_plan->transaction_manager_->Begin(exec_plan->txn);
                txn_id = exec_plan->txn->get_txn_id();
                cout << "begin txn = " << txn_id << endl; 
                ok_txt = "OK";
            } else if (auto x = std::dynamic_pointer_cast<ast::TxnCommit>(root)) {
                // commit;
                if(txn_id){
                    shared_ptr<op_executor> exec_plan(new op_executor(context));
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                    exec_plan->transaction_manager_->Commit(exec_plan->txn);
                    txn_id = 0;
                }
                cout << "commit txn = " << txn_id << endl;
                ok_txt = "OK";
            } else if (auto x = std::dynamic_pointer_cast<ast::TxnAbort>(root)) {
                if(txn_id){
                    shared_ptr<op_executor> exec_plan(new op_executor(context));
                    exec_plan->txn = exec_plan->transaction_manager_->getTransaction(txn_id);
                    exec_plan->transaction_manager_->Abort(exec_plan->txn);
                    txn_id = 0;
                }
                cout << "abort txn = " << txn_id << endl;
                ok_txt = "OK";
            }
        }
    }
    string cli_str;
    if (ok_txt != ""){
        cli_str = ok_txt;
    }
    else if(int(res.size()) != 0){
        table_to_string(res, cli_str);
    } else if(int (res_txt.size()) > 1){
        txt_to_string(res_txt, cli_str);
    }
    else {
        cli_str = "empty set \n";
    }
    return cli_str;
}