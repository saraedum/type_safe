// Copyright (C) 2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED
#define TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED

#include <type_traits>

#include <type_safe/constrained_type.hpp>

namespace type_safe
{
    namespace constraints
    {

        struct dynamic_bound { };

        namespace detail
        {
            // Base to enable empty base optimization when BoundConstant is not dynamic_bound.
            // Neccessary when T is not a class.
            template <typename T>
            struct Wrapper
            {
                using value_type = T;
                T value;
            };

            template <typename BoundConstant>
            struct is_dynamic : std::is_same<BoundConstant,dynamic_bound>
            {
            };

            template <typename T, typename BoundConstant>
            using Base = typename std::conditional<is_dynamic<BoundConstant>::value,
                                                   Wrapper<T>,
                                                   BoundConstant>::type;
        } // detail namespace

#define TYPE_SAFE_DETAIL_MAKE(Name, Op)                                                            \
    template <typename T, typename BoundConstant = dynamic_bound>                                  \
    class Name : detail::Base<T, BoundConstant>                                                    \
    {                                                                                              \
        using Base = detail::Base<T, BoundConstant>;                                               \
                                                                                                   \
        static constexpr bool is_dynamic = detail::is_dynamic<BoundConstant>::value;               \
    public:                                                                                        \
        explicit Name()                                                                            \
        {                                                                                          \
            static_assert(!is_dynamic,"constructor requires static bound");                        \
        }                                                                                          \
                                                                                                   \
        explicit Name(const T& bound) : Base{bound}                                                \
        {                                                                                          \
            static_assert(is_dynamic,"constructor requires dynamic bound");                        \
        }                                                                                          \
                                                                                                   \
        explicit Name(T&& bound) noexcept(std::is_nothrow_move_constructible<T>::value)            \
        : Base{std::move(bound)}                                                                   \
        {                                                                                          \
            static_assert(is_dynamic,"constructor requires dynamic bound");                        \
        }                                                                                          \
                                                                                                   \
        template <typename U>                                                                      \
        bool operator()(const U& u) const                                                          \
        {                                                                                          \
            return u Op get_bound();                                                               \
        }                                                                                          \
                                                                                                   \
        const T& get_bound() const noexcept                                                        \
        {                                                                                          \
            return Base::value;                                                                    \
        }                                                                                          \
    };

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is less than some given value.
        TYPE_SAFE_DETAIL_MAKE(less, <)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is less than or equal to some given value.
        TYPE_SAFE_DETAIL_MAKE(less_equal, <=)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is greater than some given value.
        TYPE_SAFE_DETAIL_MAKE(greater, >)

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is greater than or equal to some given value.
        TYPE_SAFE_DETAIL_MAKE(greater_equal, >=)

#undef TYPE_SAFE_DETAIL_MAKE

        namespace detail
        {
            // checks that that the value is less than the upper bound
            template <bool Inclusive, typename T, typename BoundConstant>
            using upper_bound_t =
                typename std::conditional<Inclusive, less_equal<T, BoundConstant>,
                                                     less<T, BoundConstant>>::type;

            // checks that the value is greater than the lower bound
            template <bool Inclusive, typename T, typename BoundConstant>
            using lower_bound_t =
                typename std::conditional<Inclusive, greater_equal<T, BoundConstant>,
                                                     greater<T, BoundConstant>>::type;
        } // namespace detail

        constexpr bool open   = false;
        constexpr bool closed = true;

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds,
        /// `LowerInclusive`/`UpperInclusive` control whether the lower/upper bound itself is valid too.
        /// `LowerConstant`/`UpperConstant` control whether the lower/upper bound is specified statically or dynamically.
        /// When one is `dynamic_bound`, its bound is specified at runtime. Otherwise, it must match
        /// the interface and semantics of `std::integral_constant<T>`, in which case its `value` is the bound.
        template <typename T, bool LowerInclusive, bool UpperInclusive,
                  typename LowerConstant = dynamic_bound,
                  typename UpperConstant = dynamic_bound>
        class bounded : detail::lower_bound_t<LowerInclusive, T, LowerConstant>,
                        detail::upper_bound_t<UpperInclusive, T, UpperConstant>
        {
            static constexpr bool lower_is_dynamic = detail::is_dynamic<LowerConstant>::value;
            static constexpr bool upper_is_dynamic = detail::is_dynamic<UpperConstant>::value;

            using Lower = detail::lower_bound_t<LowerInclusive, T, LowerConstant>;
            using Upper = detail::upper_bound_t<UpperInclusive, T, UpperConstant>;

            const Lower& lower() const noexcept { return *this; }
            const Upper& upper() const noexcept { return *this; }

            template <typename U>
            using decay_same = std::is_same<typename std::decay<U>::type, T>;

        public:
            template <typename LC = LowerConstant, typename UC = UpperConstant,
                      typename = typename std::enable_if<
                                    decay_same<typename LC::value_type>::value>::type,
                      typename = typename std::enable_if<
                                    decay_same<typename UC::value_type>::value>::type>
            bounded()
            {
                static_assert(!lower_is_dynamic && !upper_is_dynamic,
                              "constructor requires static bounds");
            }

            template <typename U, bool req = upper_is_dynamic, typename LC = LowerConstant,
                      typename = typename std::enable_if<req &&
                                    decay_same<typename LC::value_type>::value>::type,
                      typename = typename std::enable_if<decay_same<U>::value>::type>
            bounded(U&& upper)
            : Upper(std::forward<U>(upper))
            {
            }

            template <typename U, typename UC = UpperConstant, bool req = lower_is_dynamic,
                      typename = typename std::enable_if<req &&
                                    decay_same<typename UC::value_type>::value>::type,
                      typename = typename std::enable_if<decay_same<U>::value>::type>
            bounded(U&& lower)
            : Lower(std::forward<U>(lower))
            {
            }

            /// \exclude
            template <typename U, bool req = lower_is_dynamic!=upper_is_dynamic,
                      typename = typename std::enable_if<!req>::type,
                      typename = typename std::enable_if<decay_same<U>::value>::type>
            bounded(U&&)
            {
                static_assert(req,"one-argument constructors require a dynamic and static bound");
            }

            template <typename U1, typename U2,
                      typename = typename std::enable_if<decay_same<U1>::value>::type,
                      typename = typename std::enable_if<decay_same<U2>::value>::type>
            bounded(U1&& lower, U2&& upper)
            : Lower(std::forward<U1>(lower)), Upper(std::forward<U2>(upper))
            {
                static_assert(upper_is_dynamic && lower_is_dynamic,
                              "constructor requires dynamic bounds");
            }

            template <typename U>
            bool operator()(const U& u) const
            {
                return lower()(u) && upper()(u);
            }

            const T& get_lower_bound() const noexcept
            {
                return lower().get_bound();
            }

            const T& get_upper_bound() const noexcept
            {
                return upper().get_bound();
            }
        };

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds but not the bounds themselves.
        template <typename T, typename LowerConstant = dynamic_bound,
                              typename UpperConstant = dynamic_bound>
        using open_interval = bounded<T, open, open, LowerConstant, UpperConstant>;

        /// A `Constraint` for the [type_safe::constrained_type<T, Constraint, Verifier>]().
        /// A value is valid if it is between two given bounds or the bounds themselves.
        template <typename T, typename LowerConstant = dynamic_bound,
                              typename UpperConstant = dynamic_bound>
        using closed_interval = bounded<T, closed, closed, LowerConstant, UpperConstant>;
    } // namespace constraints

    /// \exclude
    namespace detail
    {
        template <typename T, typename U1, typename U2>
        struct assert_value_type
        {
            static_assert(std::is_same<T, U1>::value && std::is_same<U1, U2>::value,
                          "types of bounds in call mismatch");
        };

        using constraints::detail::is_dynamic;

        template <typename T, typename, typename, typename...>
        struct bounded_value_type_impl
        {
            static_assert(!std::is_same<T,T>::value,"call has too many runtime bounds");
        };

        template <typename T, typename LowerConstant, typename UpperConstant,
                  typename U1, typename U2>
        struct bounded_value_type_impl<T, LowerConstant, UpperConstant, U1, U2>
        : assert_value_type<T, U1, U2>
        {
            static constexpr bool lower_is_dynamic = is_dynamic<LowerConstant>::value;
            static constexpr bool upper_is_dynamic = is_dynamic<UpperConstant>::value;

            static_assert(lower_is_dynamic && upper_is_dynamic,"call has too many runtime bounds");
            using type = T;
        };

        template <typename T, typename BoundConstant>
        using value_type = typename constraints::detail::Base<T, BoundConstant>::value_type;

        template <typename T, typename LowerConstant, typename UpperConstant, typename U>
        struct bounded_value_type_impl<T, LowerConstant, UpperConstant, U>
        : assert_value_type<T, value_type<U, LowerConstant>, value_type<U, UpperConstant>>
        {
            static constexpr bool lower_is_dynamic = is_dynamic<LowerConstant>::value;
            static constexpr bool upper_is_dynamic = is_dynamic<UpperConstant>::value;

            static_assert(lower_is_dynamic!=upper_is_dynamic,"+/- 1 too many runtime bounds");
            using type = T;
        };

        template <typename T, typename LowerConstant, typename UpperConstant>
        struct bounded_value_type_impl<T, LowerConstant, UpperConstant>
        : assert_value_type<T, typename LowerConstant::value_type,
                               typename UpperConstant::value_type>
        {
            static constexpr bool lower_is_dynamic = is_dynamic<LowerConstant>::value;
            static constexpr bool upper_is_dynamic = is_dynamic<UpperConstant>::value;

            static_assert(!lower_is_dynamic && !upper_is_dynamic,"call needs runtime bounds");
            using type = T;
        };

        template <typename T, typename LowerConstant, typename UpperConstant, typename... Us>
        using bounded_value_type =
            typename bounded_value_type_impl<typename std::decay<T>::type,
                                             LowerConstant,
                                             UpperConstant,
                                             typename std::decay<Us>::type...>::type;
    } // namespace detail

    /// An alias for [type_safe::constrained_type<T, Constraint, Verifier>]() that uses [type_safe::constraints::bounded<T, LowerInclusive, UpperInclusive, LowerConstant, UpperConstant>]() as its `Constraint`.
    /// \notes This is some type where the values must be in a certain interval.
    template <typename T, bool LowerInclusive, bool UpperInclusive,
              typename LowerConstant = constraints::dynamic_bound,
              typename UpperConstant = constraints::dynamic_bound>
    using bounded_type =
        constrained_type<T, constraints::bounded<T, LowerInclusive, UpperInclusive,
                                                    LowerConstant, UpperConstant>>;

    /// \returns A [type_safe::bounded_type<T, LowerInclusive, UpperInclusive LowerConstant, UpperConstant>]() with the given `value` and lower and upper dynamic bounds, in that order,
    /// where the bounds are valid values as well.
    /// \remarks `sizeof...(Us)` shall be equal to the number of dynamic bounds.
    template <typename LowerConstant = constraints::dynamic_bound,
              typename UpperConstant = constraints::dynamic_bound,
              typename T, typename... Us>
    auto make_bounded(T&& value, Us&&... bounds)
        -> bounded_type<detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>,
                        true, true, LowerConstant, UpperConstant>
    {
        using value_type = detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>;
        return bounded_type<value_type, true, true, LowerConstant, UpperConstant>(
                    std::forward<T>(value),
                    constraints::closed_interval<value_type, LowerConstant, UpperConstant>(
                        std::forward<Us>(bounds)...));
    }

    /// \returns A [type_safe::bounded_type<T, LowerInclusive, UpperInclusive LowerConstant, UpperConstant>]() with the given `value` and lower and upper dynamic bounds, in that order,
    /// where the bounds are not valid values.
    /// \remarks `sizeof...(Us)` shall be equal to the number of dynamic bounds.
    template <typename LowerConstant = constraints::dynamic_bound,
              typename UpperConstant = constraints::dynamic_bound,
              typename T, typename... Us>
    auto make_bounded_exclusive(T&& value, Us&&... bounds)
        -> bounded_type<detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>,
                        false, false, LowerConstant, UpperConstant>
    {
        using value_type = detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>;
        return bounded_type<value_type, false, false, LowerConstant, UpperConstant>(
                    std::forward<T>(value),
                    constraints::open_interval<value_type, LowerConstant, UpperConstant>(
                        std::forward<Us>(bounds)...));
    }

    /// \effects Changes `val` so that it is in the interval.
    /// If it is not in the interval, assigns the bound that is closer to the value.
    template <typename T, typename LowerConstant, typename UpperConstant, typename U>
    void clamp(const constraints::closed_interval<T, LowerConstant, UpperConstant>& interval, U& val)
    {
        if (val < interval.get_lower_bound())
            val = static_cast<U>(interval.get_lower_bound());
        else if (val > interval.get_upper_bound())
            val = static_cast<U>(interval.get_upper_bound());
    }

    /// A `Verifier` for [type_safe::constrained_type<T, Constraint, Verifier]() that clamps the value to make it valid.
    /// It must be used together with [type_safe::constraints::less_equal<T, BoundConstant>](), [type_safe::constraints::greater_equal<T, BoundConstant>]() or [type_safe::constraints::closed_interval<T, LowerConstant, UpperConstant>]().
    struct clamping_verifier
    {
        /// \effects If `val` is greater than the bound of `p`,
        /// assigns the bound to `val`.
        /// Otherwise does nothing.
        template <typename Value, typename T, typename BoundConstant>
        static void verify(Value& val, const constraints::less_equal<T, BoundConstant>& p)
        {
            if (!p(val))
                val = static_cast<Value>(p.get_bound());
        }

        /// \effects If `val` is less than the bound of `p`,
        /// assigns the bound to `val`.
        /// Otherwise does nothing.
        template <typename Value, typename T, typename BoundConstant>
        static void verify(Value& val, const constraints::greater_equal<T, BoundConstant>& p)
        {
            if (!p(val))
                val = static_cast<Value>(p.get_bound());
        }

        /// \effects Same as `clamp(interval, val)`.
        template <typename Value, typename T, typename LowerConstant, typename UpperConstant>
        static void verify(
            Value& val,
            const constraints::closed_interval<T, LowerConstant, UpperConstant>& interval)
        {
            clamp(interval, val);
        }
    };

    /// An alias for [type_safe::constrained_type<T, Constraint, Verifier>]() that uses [type_safe::constraints::closed_interval<T, LowerConstant, UpperConstant>]() as its `Constraint`
    /// and [type_safe::clamping_verifier]() as its `Verifier`.
    /// \notes This is some type where the values are always clamped so that they are in a certain interval.
    template <typename T, typename LowerConstant = constraints::dynamic_bound,
                          typename UpperConstant = constraints::dynamic_bound>
    using clamped_type = constrained_type<T,
                                          constraints::closed_interval<T, LowerConstant,
                                                                          UpperConstant>,
                                          clamping_verifier>;

    /// \returns A [type_safe::clamped_type<T, LowerConstant, UpperConstant>]() with the given `value` and lower and upper dynamic bounds, in that order.
    /// \remarks `sizeof...(Us)` shall be equal to the number of dynamic bounds.
    template <typename LowerConstant = constraints::dynamic_bound,
              typename UpperConstant = constraints::dynamic_bound,
              typename T, typename... Us>
    auto make_clamped(T&& value, Us&&... bounds)
        -> clamped_type<detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>,
                        LowerConstant, UpperConstant>
    {
        using value_type = detail::bounded_value_type<T, LowerConstant, UpperConstant, Us...>;
        return clamped_type<value_type, LowerConstant, UpperConstant>(
                    std::forward<T>(value),
                    constraints::closed_interval<value_type, LowerConstant, UpperConstant>(
                        std::forward<Us>(bounds)...));
    }
} // namespace type_safe

#endif // TYPE_SAFE_BOUNDED_TYPE_HPP_INCLUDED