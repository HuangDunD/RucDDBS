//demo distsql engine
#include "head.h"

using namespace std;

vector<shared_ptr<Node>> Nodes;

vector<shared_ptr<record>> send_plan(string target_address, shared_ptr<Operators> node_plan){
    
    return node_plan->exec_op();
}

void parser_sql(string str){
    YY_BUFFER_STATE buf = yy_scan_string(str.c_str());
}

void build_select_plan(shared_ptr<ast::SelectStmt> select_tree, std::shared_ptr<op_excutor> exec_plan){
    shared_ptr<Operators> cur_node = exec_plan;
    // 投影
    if(select_tree->cols.size()){
        shared_ptr<op_projection> projection_plan(new op_projection);
        projection_plan->cols = select_tree->cols;
        
        cur_node->next_node = projection_plan;
        cur_node = cur_node->next_node; 
    }

    for(int i = 0; i < select_tree->conds.size(); i++){
        if(select_tree->conds.size()){
            shared_ptr<op_selection> selection_plan(new op_selection);
            selection_plan->conds = select_tree->conds[i];
            
            cur_node->next_node = selection_plan;
            cur_node = cur_node->next_node;
        }
    }

    // join
    if(select_tree->tabs.size() > 1){
        shared_ptr<op_join> join_plan(new op_join);
        int test = 1;
        if(test){
            shared_ptr<op_tablescan> scan_plan0(new op_tablescan);
            scan_plan0->tabs = "A";
            scan_plan0->par = 0;
            join_plan->tables_get.push_back(scan_plan0);

            shared_ptr<op_distribution> dist_plan1(new op_distribution);
            shared_ptr<op_tablescan> scan_plan10(new op_tablescan);
            shared_ptr<op_tablescan> scan_plan11(new op_tablescan);
            scan_plan10->tabs = "B";
            scan_plan10->par = 0;
            scan_plan11->tabs = "B";
            scan_plan11->par = 1;
            dist_plan1->nodes_plan.push_back(scan_plan10);
            dist_plan1->nodes_plan.push_back(scan_plan11);
            dist_plan1->target_address.push_back("0.0.0.0:8001");
            dist_plan1->target_address.push_back("0.0.0.0:8002");
            join_plan->tables_get.push_back(dist_plan1);
        }
        
        cur_node->next_node = join_plan;
        cur_node = cur_node->next_node;
    }
    
}

void build_plan(shared_ptr<op_excutor> exec_plan){
    if (ast::parse_tree != nullptr) {
        if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(ast::parse_tree)) {
                build_select_plan(x, exec_plan);
            }
    }
}

vector<shared_ptr<record>> Sql_execute(string str){
    parser_sql(str);
    if (yyparse() == 0) {
        ast::TreePrinter::print(ast::parse_tree);
        shared_ptr<op_excutor> exec_plan(new op_excutor);
        build_plan(exec_plan);
        auto res = exec_plan->exec_op();
    }
}

// void test_init(){
//     shared_ptr<Node> node1(new Node);
//     node1->node_name = "node1";
//     cout << "node1" << endl;
//     for(int i = 0; i < 2; i++){
//         shared_ptr<record> rec(new record);
//         for(int j = 0; j < 2;j++){
//             shared_ptr<value> val(new value);
//             val->tab_name = "tab1";
//             val->col_name = "tab1col" + to_string(j);
//             val->str = to_string(i+j);
//             rec->row.push_back(val);
//         }
//         node1->store->records.push_back(rec);
//         rec->pt_row();
//         cout << endl;
//     }
//     Nodes.push_back(node1);


//     shared_ptr<Node> node2(new Node);
//     node2->node_name = "node2";
//     cout << "node2" << endl;
//     for(int i = 0; i < 2; i++){
//         shared_ptr<record> rec(new record);
//         for(int j = 0; j < 2;j++){
//             shared_ptr<value> val(new value);
//             val->tab_name = "tab2";
//             val->col_name = "tab2col" + to_string(j);
//             val->str = to_string(i+j);
//             rec->row.push_back(val);
//         }
//         node2->store->records.push_back(rec);
//         rec->pt_row();
//         cout << endl;
//     }
//     Nodes.push_back(node2);
// }

// int main(){

//     test_init();

//     string str;
//     getline(cin,str);
//     cout << str;

//     YY_BUFFER_STATE buf = yy_scan_string(str.c_str());
//     if (yyparse() == 0) {
//         ast::TreePrinter::print(ast::parse_tree);
//         shared_ptr<op_excutor> exec_plan(new op_excutor);
//         if (ast::parse_tree != nullptr) {
//             if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(ast::parse_tree)) {
//                 build_select_plan(x, exec_plan);
//             }
//             auto res = exec_plan->exec_op();
//             for(auto iter: res){
//                 iter->pt_row();
//                 cout << endl;
//             }
//         }
//     }
//     return 0;
// }