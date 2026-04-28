#ifndef BPTREE_H
#define BPTREE_H

#include <vector>
#include <string>
#include "storage_types.h"
#include "disk_manager.h"
#include "tuple_serializer.h"

// Max keys per node is 4 (Order 5)
static const int MAX_KEYS = 4;
static const int MAX_CHILDREN = 5;

/**
 * Node structure for the B+ Tree.
 * Persistent version utilizing TupleSerializer for binary conversion.
 */
struct BPtreeNode {
    bool is_leaf;
    int num_keys;
    uint32_t page_id;
    int keys[MAX_KEYS];

    // Internal Nodes
    uint32_t children[MAX_CHILDREN];

    // Leaf Nodes
    RID values[MAX_KEYS];
    uint32_t next_page_id;
    uint32_t prev_page_id;

    BPtreeNode(uint32_t id = INVALID_PAGE_ID, bool leaf = false);

    // Persistence using project's TupleSerializer
    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

  private:
    // Schema used for Node serialization
    static std::vector<ColumnSchema> GetNodeSchema(bool leaf);
};

/**
 * B+ Tree implementation managing RIDs.
 * Persistent version using DiskManager and TupleSerializer.
 */
class BPtree {
  public:
    BPtree(const char* table_name);
    ~BPtree();

    void insert(int key, RID rid);
    int insert_record(int key, int record_num);
    int get_record(int key);
    RID search(int key);
    bool update_rid(int key, RID new_rid);
    void print();

  private:
    DiskManager* disk_manager_;
    uint32_t root_page_id_;
    std::string table_name_;

    void load_root();
    void save_root();
    uint32_t allocate_node(bool is_leaf);
    void read_node(uint32_t page_id, BPtreeNode& node);
    void write_node(const BPtreeNode& node);

    void insert_into_leaf(BPtreeNode& leaf, int key, RID rid);
    void split_leaf(BPtreeNode& leaf, int key, RID rid);
    void insert_into_parent(uint32_t old_node_id, int key, uint32_t new_node_id);
    void split_internal(BPtreeNode& parent, int key, uint32_t new_node_id);

    void print_node(uint32_t page_id, int level);
};

#endif
