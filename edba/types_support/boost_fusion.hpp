#ifndef EDBA_TYPES_SUPPORT_BOOST_FUSION_HPP
#define EDBA_TYPES_SUPPORT_BOOST_FUSION_HPP

#include <edba/statement.hpp>

#include <boost/fusion/support/is_sequence.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>

namespace edba {

#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include <boost/preprocessor/repetition/enum_params.hpp>

template<typename T>
struct bind_conversion< T, typename boost::enable_if< boost::fusion::traits::is_sequence<T> >::type >
{
    struct helper
    {
        statement& st_;
        helper(statement& st) : st_(st) {}

        template<typename T1>
        void operator()(const T1& v) const
        {
            const_cast<statement&>(st_) << v;
        }
    };

    template<typename ColOrName>
    static void bind(statement& st, ColOrName, const T& v)
    {
        helper h(st);
        boost::fusion::for_each(v, h);
    }
};

template<typename T>
struct fetch_conversion< T, typename boost::enable_if< boost::fusion::traits::is_sequence<T> >::type >
{
    struct helper
    {
        const row& r_;
        helper(const row& r) : r_(r) {}

        template<typename T1>
        void operator()(T1& v) const
        {
            r_ >> v;
        }
    };

    template<typename ColOrName>
    static bool fetch(const row& r, ColOrName, T& v)
    {
        boost::fusion::for_each(v, helper(r));
        return true;
    }
};

}             // namespace edba

#endif        // EDBA_TYPES_SUPPORT_BOOST_FUSION_HPP
