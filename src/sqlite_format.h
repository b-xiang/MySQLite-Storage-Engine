#ifndef _SQLITE_FORMAT_H_
#define _SQLITE_FORMAT_H_


#include "utils.h"


/*
** Constants
*/
#define SQLITE3_SIGNATURE_SIZE 16
#define SQLITE3_SIGNATURE "SQLite format 3"

#define DB_HEADER_SIZE 100

#define DBHDR_PGSIZE_OFFSET 16
#define DBHDR_PGSIZE_LEN 2

#define BTREEHDR_BTREETYPE_OFFSET 0

#define BTREEHDR_FREEBLOCKOFST_OFFSET 1
#define BTREEHDR_FREEBLOCKOFST_LEN 2

#define BTREEHDR_NCELL_OFFSET 3
#define BTREEHDR_NCELL_LEN 2

#define BTREEHDR_CELLCONTENTAREAOFST_OFFSET 5
#define BTREEHDR_CELLCONTENTAREAOFST_LEN 2

#define BTREEHDR_NFRAGMENTATION_OFFSET 7

#define BTREEHDR_RIGHTMOSTPG_OFFSET 8
#define BTREEHDR_RIGHTMOSTPG_LEN 4

#define SQLITE3_VARINT_MAXLEN 9

typedef enum btree_page_type {
  INDEX_INTERIOR     = 2,
  TABLE_INTERIOR     = 5,
  INDEX_LEAF         = 10,
  TABLE_LEAF         = 13,
} btree_page_type;


static inline bool has_sqlite3_signature(FILE * const f)
{
  char s[SQLITE3_SIGNATURE_SIZE];
  if (!mysqlite_fread(s, 0, SQLITE3_SIGNATURE_SIZE, f)) return false;
  return strcmp(s, SQLITE3_SIGNATURE) == 0;
}


/*
** pathをopenし，もし存在すればそれがSQLite DBとしてvalidなものかをチェックする
**
** @return
** NULL: Error. `message' is set.
*/
static inline FILE *open_sqlite_db(const char * const path,
                                   /* out */
                                   bool * const is_existing_db,
                                   char * const message)
{
  struct stat st;
  if (stat(path, &st) == 0) {
    *is_existing_db = true;
    FILE *f = fopen(path, "r");
    if (!f) {
      sprintf(message, "Permission denied: Cannot open %s in read mode.", path);
      return NULL;
    }
    if (!has_sqlite3_signature(f)) {
      sprintf(message, "Format error: %s does not seem SQLite3 database.", path);
      return NULL;
    }
    return f;
  } else {
    *is_existing_db = false;
    FILE *f = fopen(path, "w+");
    if (!f) {
      sprintf(message, "Permission denied: Cannot create %s.", path);
      return NULL;
    }
    return f;
  }
}
static inline FILE *open_sqlite_db(const char * const path,
                                   /* out */
                                   bool * const is_existing_db)
{
  char msg[1024];
  FILE *f_ret = open_sqlite_db(path, is_existing_db, msg);
  if (!f_ret) log("%s", msg);
  return f_ret;
}
static inline FILE *open_sqlite_db(const char * const existing_path)
{
  bool is_existing_db;
  FILE *f_ret = open_sqlite_db(existing_path, &is_existing_db);
  if (!f_ret) return NULL;

  if (!is_existing_db) {
    if (0 != fclose(f_ret)) perror("fclose() failed\n");
    return NULL;
  }

  return f_ret;
}


/*
** Database header
*/
class DbHeader {
private:
  FILE * const f_db;
  u8 *hdr_data;

public:
  /*
  ** @note
  ** Constructor does not read(2) page contents.
  ** Call this->init() after object is constructed.
  */
  DbHeader(FILE * const f_db)
    : f_db(f_db)
  {
    assert(f_db);
    assert(hdr_data = (u8 *)malloc(DB_HEADER_SIZE));
  }
  ~DbHeader() {
    free(hdr_data);
  }

  bool read() const {
    return mysqlite_fread(hdr_data, 0, DB_HEADER_SIZE, f_db);
  }

  u16 get_pg_size() const {
    return u8s_to_val<u16>(&hdr_data[DBHDR_PGSIZE_OFFSET], DBHDR_PGSIZE_LEN);
  }

private:
  // Prohibit default constructor
  DbHeader() : f_db(NULL) {}
};

/*
** Page class
*/
class Page {
protected:
  FILE * const f_db;
  u8 *pg_data;
public:
  const DbHeader * const db_header;
  u32 pg_id;

public:
  /*
  ** @note
  ** Constructor does not read(2) page contents.
  ** Call this->init() after object is constructed.
  */
  Page(FILE * const f_db,
       const DbHeader * const db_header,
       u32 pg_id)
    : f_db(f_db), db_header(db_header), pg_id(pg_id)
  {
    assert(f_db);
    assert(db_header);
    assert(pg_data = (u8 *)malloc(db_header->get_pg_size()));
  }
  virtual ~Page() {
    free(pg_data);
  }

  bool read() const {
    assert(db_header);
    u16 pg_size = db_header->get_pg_size();
    return mysqlite_fread(pg_data, pg_size * (pg_id - 1), pg_size, f_db);
  }

private:
  // Prohibit default constructor
  Page() : f_db(NULL), db_header(NULL) {}
};

/*
** Btree page virtual class
*/
class BtreePage : public Page {
public:
  BtreePage(FILE * const f_db,
            const DbHeader * const db_header,
            u32 pg_id)
    : Page(f_db, db_header, pg_id)
  {}

  // Btree header info

  btree_page_type get_btree_type() const {
    return (btree_page_type)pg_data[BTREEHDR_BTREETYPE_OFFSET];
  }

  u16 get_freeblock_offset() const {
    return u8s_to_val<u16>(&pg_data[BTREEHDR_FREEBLOCKOFST_OFFSET],
                           BTREEHDR_FREEBLOCKOFST_LEN);
  }

  u16 get_n_cell() const {
    return u8s_to_val<u16>(&pg_data[BTREEHDR_NCELL_OFFSET],
                           BTREEHDR_NCELL_LEN);
  }

  u16 get_cell_content_area_offset() const {
    return u8s_to_val<u16>(&pg_data[BTREEHDR_CELLCONTENTAREAOFST_OFFSET],
                           BTREEHDR_CELLCONTENTAREAOFST_LEN);
  }

  u8 get_n_fragmentation() const {
    return pg_data[BTREEHDR_NFRAGMENTATION_OFFSET];
  }

  u32 get_rightmost_pg() const {
    assert(get_btree_type() == INDEX_INTERIOR ||
           get_btree_type() == TABLE_INTERIOR);
    return u8s_to_val<u32>(&pg_data[BTREEHDR_RIGHTMOSTPG_OFFSET],
                           BTREEHDR_RIGHTMOSTPG_LEN);
  }

protected:
  bool is_valid_hdr() const {
    bool is_valid = true;
    u16 pg_size = db_header->get_pg_size();
    {
      btree_page_type type = get_btree_type();
      is_valid &= (type == INDEX_INTERIOR ||
                   type == TABLE_INTERIOR ||
                   type == INDEX_LEAF ||
                   type == TABLE_LEAF);
    }
    {
      u16 offset = get_freeblock_offset();
      is_valid &= (0 <= offset && offset < pg_size);
    }
    {
      u16 n_cell = get_n_cell();
      is_valid &= (0 <= n_cell && n_cell < pg_size);
    }
    {
      u16 offset = get_cell_content_area_offset();
      is_valid &= (0 <= offset && offset <= pg_size);
      // cell_content_area_offset == pg_size means
      // there are no cells yet
    }
    {
      u16 n_fragmentation = get_n_fragmentation();
      is_valid &= (0 <= n_fragmentation && n_fragmentation < pg_size);
    }
    return is_valid;
  }

public:
  // Page management
  bool read() const {
    return Page::read() && is_valid_hdr();
  }

protected:
  virtual u32 get_ith_cell_offset() = 0;
};

/*
** Table leaf page
*/
class TableLeafPage : public BtreePage {
public:
 TableLeafPage(FILE * const f_db,
               const DbHeader * const db_header,
               u32 pg_id)
   : BtreePage(f_db, db_header, pg_id)
 {}
};


#endif /* _SQLITE_FORMAT_H_ */