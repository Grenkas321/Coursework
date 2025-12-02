#pragma once

#include <iostream>
#include <string>
#include <memory>

namespace scheduling_problem
{
    /**
     * Enumerates the possible runtime types a `parameter` can hold.
     */
    enum class value_t : std::uint8_t
    {
        /** Unsigned integer value. */
        number_integer_unsigned,
        /** Signed integer value. */
        number_integer,
        /** Floating-point (double) value. */
        number_float,
        /** Boolean value. */
        boolean,
        /** String value. */
        string,
        /** Object value (reserved / not used by this implementation). */
        object
    };

    /**
     * A small tagged union for algorithm parameters.
     *
     * The value can be one of: unsigned, int, double, bool, std::string.
     * Designed for parsing algorithm parameters from configuration and
     * for building parameter dictionaries at initialization time.
     */
    class parameter
    {
    private:
        /**
         * Internal union storing the active value.
         * Lifetime of the active member is managed via `type_`
         * and `destroy(value_t)`.
         */
        union param_value
        {
            /** Active value when type_ == number_integer_unsigned. */
            unsigned u_number;
            /** Active value when type_ == number_integer. */
            int number;
            /** Active value when type_ == number_float. */
            double number_float;
            /** Active value when type_ == boolean. */
            bool boolean;
            /** Active value when type_ == string. */
            std::string *string;

            /**
             * Default-initialize the storage (no active value).
             */
            param_value();

            /**
             * Initialize with an unsigned integer.
             *
             * @param u_number  Initial value.
             */
            param_value(unsigned u_number);

            /**
             * Initialize with a signed integer.
             *
             * @param number  Initial value.
             */
            param_value(int number);

            /**
             * Initialize with a floating-point number.
             *
             * @param number_float  Initial value.
             */
            param_value(double number_float);

            /**
             * Initialize with a boolean value.
             *
             * @param boolean  Initial value.
             */
            param_value(bool boolean);

            /**
             * Initialize with a string (heap-allocated).
             *
             * @param s  Initial string.
             */
            param_value(const std::string &s);

            /**
             * Initialize with a C-string (heap-allocated).
             *
             * @param s  Null-terminated string.
             */
            param_value(const char *s);

            /**
             * Initialize storage according to the given type
             * without setting a concrete value.
             *
             * @param t  Target type.
             */
            param_value(value_t t);

            /**
             * Destroy the currently active member matching type t.
             *
             * @param t  Active type to destroy.
             */
            void destroy(value_t t);

            /**
             * Trivial destructor; does not destroy active member.
             * Use destroy(type_) before calling.
             */
            ~param_value();
        };

    private:
        /** Backing storage for the active value. */
        param_value value_;
        /** Discriminator describing which member of `value_` is active. */
        value_t type_;

    public:
        /**
         * Construct an empty parameter with type `object` by default.
         * The exact default may be implementation-defined.
         */
        parameter();

        /**
         * Construct a parameter from raw storage and explicit type.
         *
         * @param value  Pre-initialized union storage.
         * @param type   Discriminator describing the active member.
         */
        parameter(const param_value &value, value_t type);

        /**
         * Copy-construct from another parameter, preserving type and value.
         *
         * @param other  Source parameter.
         */
        parameter(const parameter &other);

        /**
         * Construct from a signed integer.
         *
         * @param num  Initial value.
         */
        parameter(int num);

        /**
         * Construct from an unsigned integer.
         *
         * @param u_num  Initial value.
         */
        parameter(unsigned u_num);

        /**
         * Construct from a floating-point number.
         *
         * @param f_num  Initial value.
         */
        parameter(double f_num);

        /**
         * Construct from a boolean.
         *
         * @param b  Initial value.
         */
        parameter(bool b);

        /**
         * Construct from a std::string (copied).
         *
         * @param s  Initial string.
         */
        parameter(const std::string &s);

        /**
         * Construct from a C-string (copied).
         *
         * @param s  Null-terminated string.
         */
        parameter(const char *s);

        /**
         * Copy-assign from another parameter.
         * Properly destroys the current value and copies the other's value and type.
         *
         * @param other  Source parameter.
         * @return Newly assigned parameter.
         */
        parameter operator=(const parameter &other);

        /**
         * Add two numeric parameters and return the result.
         * Valid when both operands are numeric (int/unsigned/double).
         * May perform implicit widening to double.
         *
         * @param other  RHS parameter.
         * @return Result parameter.
         */
        parameter operator+(const parameter &other);

        /**
         * Subtract two numeric parameters and return the result.
         * Valid when both operands are numeric (int/unsigned/double).
         *
         * @param other  RHS parameter.
         * @return Result parameter.
         */
        parameter operator-(const parameter &other);

        /**
         * Multiply two numeric parameters and return the result.
         * Valid when both operands are numeric (int/unsigned/double).
         *
         * @param other  RHS parameter.
         * @return Result parameter.
         */
        parameter operator*(const parameter &other);

        /**
         * In-place addition of a numeric RHS.
         *
         * @param other  RHS parameter.
         * @return *this after modification.
         */
        parameter operator+=(const parameter &other);

        /**
         * In-place subtraction of a numeric RHS.
         *
         * @param other  RHS parameter.
         * @return *this after modification.
         */
        parameter operator-=(const parameter &other);

        /**
         * In-place multiplication by a numeric RHS.
         *
         * @param other  RHS parameter.
         * @return *this after modification.
         */
        parameter operator*=(const parameter &other);

        /**
         * Implicit conversion to unsigned integer.
         * Behavior is defined only when type_ is numeric or boolean.
         *
         * @return Unsigned representation of the value.
         */
        operator unsigned() const;

        /**
         * Implicit conversion to double.
         * Behavior is defined only when type_ is numeric or boolean.
         *
         * @return Double representation of the value.
         */
        operator double() const;

        /**
         * Implicit conversion to bool.
         * Numeric or string values may be converted according to implementation rules.
         *
         * @return Boolean representation of the value.
         */
        operator bool() const;

        /**
         * Implicit conversion to std::string.
         * Numeric and boolean values are stringified; strings are returned as-is.
         *
         * @return String representation of the value.
         */
        operator std::string() const;

        /**
         * Get the current runtime type of the stored value.
         *
         * @return Discriminator from value_t.
         */
        value_t type() const;

        /**
         * Destroy the active value and release any owned resources.
         */
        ~parameter();

        /**
         * Stream output helper printing the value according to its type.
         *
         * @param out    Output stream.
         * @param param  Parameter to print.
         * @return Reference to the output stream.
         */
        friend std::ostream &operator<<(std::ostream &out, const parameter &param);
    };

    /**
     * Free function overload for streaming a `parameter` to an ostream.
     *
     * @param out    Output stream.
     * @param param  Parameter to print.
     * @return Reference to the output stream.
     */
    std::ostream &operator<<(std::ostream &out, const parameter &param);
}
