
#include <functional>
#include <utility>
#include <algorithm>
#include <iostream>
#include <memory>
#include <type_traits>
#include <fmt/core.h>
#include <string>
#include <vector>
#include <array>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Parent {

    public:
        virtual ~Parent() = 0;
        virtual std::string apply(std::string const&pInput) = 0;
};
Parent::~Parent() {}

template<typename X, typename... Args>
class Child : public Parent {

    public:

        using InternalTuple = std::tuple<std::decay_t<Args>...>;
        using InternalFunc = std::function<X(Args...)>;


        Child(InternalFunc f, std::vector<std::string> const&args)
            : func{f}, argnames{args}
        {
        }

        ~Child() = default;

        template<std::size_t... ids>
        void
        unpack(json const&j, std::index_sequence<ids...> const&)
        {

            ([&]
            {
                // Get the reference removed type
                using TType = typename std::tuple_element<ids, InternalTuple>::type;
                // Get the name of the argument at that index
                std::string const&name = argnames.at(ids);

                // Extract the type with that name from the JSON and try to coerce to
                // correct type.
                auto v = j[name].get<TType>();

                // Update the internal tuple with the value.
                std::get<ids>(argvals) = v;

            } (), ...);
        }

        template<std::size_t... ids>
        void
        helper(json &outputs, std::index_sequence<ids...> const&)
        {
            ([&]
            {
                if constexpr (non_const_ref.at(ids)) {
                    outputs[argnames.at(ids)] = std::get<ids>(argvals);
                }
            } (), ...);
        }

        std::string
        apply(std::string const&s)
        {
            json inputs = json::parse(s);
            json outputs = {
                {"rc", 0 },
            };

            unpack(inputs, std::make_index_sequence<sizeof...(Args)>());
            if constexpr (std::is_void<X>::value) {
                (*this)();
            } else {
                outputs["rc"] = (*this)();
            }
            helper(outputs, std::make_index_sequence<sizeof...(Args)>());

            return outputs.dump();
        }

        template<typename T>
        std::size_t
        ih(T const&v)
        {
            std::hash<T> hasher;
            return hasher(v);
        }

        template<std::size_t... ids>
        std::vector<std::size_t>
        tupleHash(InternalTuple const&t, std::index_sequence<ids...> const&)
        {
            std::vector<std::size_t> a;

            // { ih(std::get<0>(t), ih(std::get<1>(t), ih(std::get<2>(t) }
            for (auto const& v : { ih(std::get<ids>(t))...} ) {
                a.emplace_back(v);
            }

            return a;
        }

        auto operator()() -> X
        {
            // tupleHash(argvals, 1, 2, 3, 4, 5, 6)
            if constexpr (std::is_void<X>::value) {
                std::apply(func, argvals);
            } else {
                return std::apply(func, argvals);
            }

            //cvIndices(std::make_index_sequence<sizeof...(Args)>());
            //std::for_each(v.begin(), v.end(), [](std::size_t x){ fmt::print("{}\n", x); });
        };

        InternalTuple argvals;
        InternalFunc func;
        std::vector<std::string> argnames;


        template<typename T>
        static constexpr bool
        is_nonconst_ref()
        {
            return std::is_reference<T>::value &&
                !std::is_const<typename std::remove_reference<T>::type>::value;
        }
        static constexpr std::array<bool, sizeof...(Args)> non_const_ref = {
            is_nonconst_ref<Args>()...
        };
};

template<typename X, typename... Args>
std::unique_ptr<Parent>
make_functor(X (*func)(Args...), std::vector<std::string> const&v)
{
    //std::function<X(Args...)> f = func;
    return std::make_unique<Child<X, Args...> >(std::function(func), v);
}

int
func(int &a, int b)
{
    int const r = a*b;
    a = r/2;
    return r;
}

void
func2(int &a, int b, int const&c)
{
    int const r = a*b*c;
    a = r;
}

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fmt::print("Hello!\n");

    std::vector<std::string> args{"a", "b"};
    fmt::print("func\n");
    auto p = make_functor(func, args);

    fmt::print("int& is reference: {}\n", std::is_reference<int&>::value);
    fmt::print("int const& is constref: {}\n", std::is_const<typename std::remove_reference<int const&>::type>::value);

    json jArgs = {
        {"a", 22},
        {"b", 11},
    };
    fmt::print("inputs are:\n{}\n", jArgs.dump(4));
    auto resStr = p->apply(jArgs.dump());
    json res = json::parse(resStr);
    fmt::print("outputs are:\n{}\n", res.dump(4));
#if 0
    std::tuple<int, int> vv{1, 2};
    int o = std::apply(func, vv);
    fmt::print("a = {}, b = {}, o = {}\n", std::get<0>(vv), std::get<1>(vv), o);
#endif
    fmt::print("func2\n");
    std::vector<std::string> args2{"a", "b", "c"};
    auto q = make_functor(func2, args2);


    return 0;
}
