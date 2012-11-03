#define EDBA_DRIVER_SOURCE
#include <sqlite3.h>

#include <edba/backend.hpp>
#include <edba/errors.hpp>
#include <edba/utils.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <sstream>
#include <limits>
#include <iomanip>
#include <map>

#include <stdlib.h>
#include <string.h>

namespace edba { namespace sqlite3_backend {

static std::string g_backend_name("sqlite3");
static std::string g_engine_name("sqlite3");
static int g_ver_major = (sqlite3_libversion_number() / 1000) % 1000;
static int g_ver_minor = sqlite3_libversion_number() % 1000;
static std::string g_description = std::string("SQLite Version ") + sqlite3_libversion();

class result : public backend::result {
public:
    result(sqlite3_stmt *st,sqlite3 *conn) : 
        st_(st),
        conn_(conn),
        cols_(-1)
    {
        cols_=sqlite3_column_count(st_);
    }
    virtual ~result() 
    {
        st_ = 0;
    }
    virtual next_row has_next()
    {
        return next_row_unknown;
    }
    virtual bool next() 
    {
        int r = sqlite3_step(st_);
        if(r==SQLITE_DONE)
            return false;
        if(r!=SQLITE_ROW) {
            throw edba_error(std::string("sqlite3:") + sqlite3_errmsg(conn_));
        }
        return true;
    }
    template<typename T>
    bool do_fetch(int col,T &v)
    {
        if(do_is_null(col))
            return false;
        if(sqlite3_column_type(st_,col)==SQLITE_NULL)
            return false;
        sqlite3_int64 rv = sqlite3_column_int64(st_,col);
        T tmp;
        if(std::numeric_limits<T>::is_signed) {
            tmp=static_cast<T>(rv);
            if(static_cast<sqlite3_int64>(tmp)!=rv)
                throw bad_value_cast();
        }
        else {
            if(rv < 0)
                throw bad_value_cast();
            unsigned long long urv = static_cast<unsigned long long>(rv);
            tmp=static_cast<T>(urv);
            if(static_cast<unsigned long long>(tmp)!=urv)
                throw bad_value_cast();
        }
        v=tmp;
        return true;
    }

    virtual bool fetch(int col,short &v) 
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,unsigned short &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,int &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,unsigned &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,long &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,unsigned long &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,long long &v)
    {
        return do_fetch(col,v);
    }
    virtual bool fetch(int col,unsigned long long &v)
    {
        return do_fetch(col,v);
    }
    template<typename T>
    bool do_real_fetch(int col,T &v)
    {
        if(do_is_null(col))
            return false;
        v=static_cast<T>(sqlite3_column_double(st_,col));
        return true;
    }
    virtual bool fetch(int col,float &v) 
    {
        return do_real_fetch(col,v);
    }
    virtual bool fetch(int col,double &v)
    {
        return do_real_fetch(col,v);
    }
    virtual bool fetch(int col,long double &v)
    {
        return do_real_fetch(col,v);
    }
    virtual bool fetch(int col,std::string &v)
    {
        if(do_is_null(col))
            return false;
        char const *txt = (char const *)sqlite3_column_text(st_,col);
        int size = sqlite3_column_bytes(st_,col);
        v.assign(txt,size);
        return true;
    }
    virtual bool fetch(int col,std::ostream &v)
    {
        if(do_is_null(col))
            return false;
        char const *txt = (char const *)sqlite3_column_text(st_,col);
        int size = sqlite3_column_bytes(st_,col);
        v.write(txt,size);
        return true;
    }
    virtual bool fetch(int col,std::tm &v)
    {
        if(do_is_null(col))
            return false;
        v=parse_time((char const *)(sqlite3_column_text(st_,col)));
        return true;
    }
    virtual bool is_null(int col)
    {
        return do_is_null(col);
    }
    virtual int cols() 
    {
        return cols_;
    }
    virtual unsigned long long rows()
    {
        return unsigned long long(-1);
    }
    virtual int name_to_column(const string_ref& n)
    {
        if(column_names_.empty()) 
        {
            for(int i=0;i<cols_;i++) 
            {
                char const *name = sqlite3_column_name(st_,i);
                if(!name)
                    throw std::bad_alloc();

                column_names_[name]=i;
            }
        }

        column_names_map::const_iterator p = column_names_.find(n);
        return p == column_names_.end() ? -1 : p->second;
    }
    virtual std::string column_to_name(int col)
    {
        check(col);
        char const *name = sqlite3_column_name(st_,col);
        if(!name) {
            throw std::bad_alloc();
        }
        return name;
    }
private:
    bool do_is_null(int col)
    {
        check(col);
        return sqlite3_column_type(st_,col)==SQLITE_NULL;
    }
    void check(int col)
    {
        if(col < 0 || col >= cols_)
            throw invalid_column();
    }
    sqlite3_stmt *st_;
    sqlite3 *conn_;

    typedef std::map<string_ref, int, string_ref::iless> column_names_map;
    column_names_map column_names_;
    bool column_names_prepared_;
    int cols_;
};

class statement : public backend::statement {
public:
    virtual void reset_impl()
    {
        reset_stat();
        sqlite3_clear_bindings(st_);
    }
    void reset_stat()
    {
        if(!reset_) {
            sqlite3_reset(st_);
            reset_=true;
        }
    }
    virtual void bind_impl(int col,string_ref const &v) 
    {
        reset_stat();
        check_bind(sqlite3_bind_text(st_, col, v.begin(), int(v.size()), SQLITE_TRANSIENT));
    }
    virtual void bind_impl(int col,std::tm const &v)
    {
        reset_stat();
        std::string tmp = format_time(v);
        check_bind(sqlite3_bind_text(st_, col, tmp.c_str(), int(tmp.size()), SQLITE_TRANSIENT));
    }
    virtual void bind_impl(int col,std::istream &v) 
    {
        reset_stat();
        // TODO Fix me
        std::ostringstream ss;
        ss<<v.rdbuf();
        std::string tmp = ss.str();
        check_bind(sqlite3_bind_text(st_, col, tmp.c_str(), int(tmp.size()), SQLITE_TRANSIENT));
    }
    virtual void bind_impl(int col,int v) 
    {
        reset_stat();
        check_bind(sqlite3_bind_int(st_,col,v));
    }
    template<typename IntType>
    void do_bind(int col,IntType value)
    {
        reset_stat();
        int r;
        if(sizeof(value) > sizeof(int) || (long long)(value) > std::numeric_limits<int>::max())
            r = sqlite3_bind_int64(st_,col,static_cast<sqlite3_int64>(value));
        else
            r = sqlite3_bind_int(st_,col,static_cast<int>(value));
        check_bind(r);
    }
    virtual void bind_impl(int col,unsigned v) 
    {
        do_bind(col,v);
    }
    virtual void bind_impl(int col,long v)
    {
        do_bind(col,v);
    }
    virtual void bind_impl(int col,unsigned long v)
    {
        do_bind(col,v);
    }
    virtual void bind_impl(int col,long long v)
    {
        do_bind(col,v);
    }
    virtual void bind_impl(int col,unsigned long long v)
    {
        do_bind(col,v);
    }
    virtual void bind_impl(int col,double v)
    {
        reset_stat();
        check_bind(sqlite3_bind_double(st_,col,v));
    }
    virtual void bind_impl(int col,long double v) 
    {
        reset_stat();
        check_bind(sqlite3_bind_double(st_,col,static_cast<double>(v)));
    }
    virtual void bind_null_impl(int col)
    {
        reset_stat();
        check_bind(sqlite3_bind_null(st_,col));
    }
    virtual boost::intrusive_ptr<edba::backend::result> query_impl()
    {
        reset_stat();
        reset_ = false;
        return boost::intrusive_ptr<result>(new result(st_,conn_));
    }
    virtual long long sequence_last(std::string const &/*name*/)
    {
        return sqlite3_last_insert_rowid(conn_);
    }
    virtual void exec_impl()
    {
        reset_stat();
        reset_ = false;
        int r = sqlite3_step(st_);
        if(r!=SQLITE_DONE) {
            if(r==SQLITE_ROW) {
                throw edba_error("Using exec with query!");
            }
            else 
                check_bind(r);
        }
    }
    virtual unsigned long long affected()
    {
        return sqlite3_changes(conn_);
    }
    statement(const string_ref& query,sqlite3 *conn, session_monitor* sm) : 
        backend::statement(sm, query),
        st_(0),
        conn_(conn),
        reset_(true)
    {
        if(sqlite3_prepare_v2(conn_, query.begin(), int(query.size()), &st_, 0)!=SQLITE_OK)
            throw edba_error(sqlite3_errmsg(conn_));
    }
    ~statement()
    {
        sqlite3_finalize(st_);
    }

private:
    void check_bind(int v)
    {
        if(v==SQLITE_RANGE) {
            throw invalid_placeholder(); 
        }
        if(v!=SQLITE_OK) {
            throw edba_error(sqlite3_errmsg(conn_));
        }
    }
    sqlite3_stmt *st_;
    sqlite3 *conn_;
    bool reset_;
};

class connection : public backend::connection {
public:
    connection(const conn_info& ci, session_monitor* si) : backend::connection(ci, si), conn_(0)
    {
        std::string dbname = ci.get_copy("db");
        if(dbname.empty()) {
            throw edba_error("sqlite3:database file (db propery) not specified");
        }

        string_ref mode = ci.get("mode", "create");

        int flags = 0;
        if(boost::algorithm::iequals(mode, "create"))
            flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        else if(boost::iequals(mode, "readonly"))
            flags = SQLITE_OPEN_READONLY;
        else if(boost::iequals(mode, "readwrite"))
            flags = SQLITE_OPEN_READWRITE;
        else {
            throw edba_error("sqlite3:invalid mode propery, expected "
                " 'create' (default), 'readwrite' or 'readonly' values");
        }

        std::string vfs = ci.get_copy("vfs");
        char const *cvfs = vfs.empty() ? (char const *)(0) : vfs.c_str();

        if(sqlite3_open_v2(dbname.c_str(),&conn_,flags,cvfs) != SQLITE_OK) {
            if(conn_ == 0) {
                throw edba_error("sqlite3:failed to create db object");
            }
            std::string error_message;
            try { error_message = sqlite3_errmsg(conn_); }catch(...){}
            sqlite3_close(conn_);
            conn_ = 0;
            throw edba_error("sqlite3:Failed to open connection:" + error_message);
        }
    }
    virtual ~connection() 
    {
        sqlite3_close(conn_);
    }
    virtual void begin_impl()
    {
        fast_exec("begin");	
    }
    virtual void commit_impl() 
    {
        fast_exec("commit");
    }
    virtual void rollback_impl()
    {
        fast_exec("rollback");
    }
    virtual boost::intrusive_ptr<backend::statement> prepare_statement(const string_ref& q)
    {
        return boost::intrusive_ptr<backend::statement>(new statement(q,conn_, sm_));
    }
    virtual boost::intrusive_ptr<backend::statement> create_statement(const string_ref& q)
    {
        return prepare_statement(q);
    }
    virtual void exec_batch_impl(const string_ref& q)
    {
        if (expand_conditionals_) 
            fast_exec(q.begin());
        else
        {
            // copy whole string to make it null terminated :(
            std::string tmp(q.begin(), q.end());
            fast_exec(tmp.c_str());
        }
    }
    virtual std::string escape(std::string const &s)
    {
        return escape(s.c_str(),s.c_str()+s.size());
    }
    virtual std::string escape(char const *s)
    {
        return escape(s,s+strlen(s));
    }
    virtual std::string escape(char const *b,char const *e)
    {
        std::string result;
        result.reserve(e-b);
        for(;b!=e;b++) {
            char c=*b;
            if(c=='\'')
                result+="''";
            else
                result+=c;
        }
        return result;
    }

    virtual const std::string& backend()
    {
        return g_backend_name;
    }
    virtual const std::string& engine()
    {
        return g_engine_name;
    }
    virtual void version(int& major, int& minor)
    {
        major = g_ver_major;
        minor = g_ver_minor;
    }
    virtual const std::string& description()
    {
        return g_description;
    }

private:
    void fast_exec(char const *query)
    {
        if(sqlite3_exec(conn_,query,0,0,0)!=SQLITE_OK) {
            throw edba_error(std::string("sqlite3:") + sqlite3_errmsg(conn_));
        }
    }

    sqlite3 *conn_;
};

}} // edba, sqlite3_backend

extern "C" {
    EDBA_DRIVER_API edba::backend::connection *edba_sqlite3_get_connection(const edba::conn_info& cs, edba::session_monitor* sm)
    {
        return new edba::sqlite3_backend::connection(cs, sm);
    }
}