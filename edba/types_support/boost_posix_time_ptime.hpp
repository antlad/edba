#ifndef EDBA_TYPES_SUPPORT_BOOST_POSIX_TIME_PTIME_HPP
#define EDBA_TYPES_SUPPORT_BOOST_POSIX_TIME_PTIME_HPP

#include <edba/statement.hpp>

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

namespace edba
{

template<>
struct bind_conversion<boost::posix_time::ptime, void>
{
    template<typename ColOrName>
    static void bind(statement& st, ColOrName col_or_name, const boost::posix_time::ptime& v)
    {
        st.bind(col_or_name, boost::posix_time::to_tm(v));
    }
};

template<>
struct fetch_conversion<boost::posix_time::ptime, void>
{
    template<typename ColOrName>
    static bool fetch(const row& res, ColOrName col_or_name, boost::posix_time::ptime& v)
    {
        std::tm tm_struct;
        bool ret = res.fetch(col_or_name, tm_struct);
        if (ret)
            v = boost::posix_time::ptime_from_tm(tm_struct);

        return ret;
    }
};

}

#endif // EDBA_TYPES_SUPPORT_BOOST_POSIX_TIME_PTIME_HPP
