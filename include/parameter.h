#pragma once

#include <iostream>
#include <string>
#include <memory>

namespace scheduling_problem
{
    /**
     * Enumeration of possible types of the parameters
     */
    enum class value_t : std::uint8_t
    {
        /**
         * Unsigned integer as parameter
         */
        number_integer_unsigned,
        /**
         * Signed integer as parameter
         */
        number_integer,
        /**
         * Float as parameter
         */
        number_float,
        /**
         * Boolean as parameter
         */
        boolean,
        /**
         * String as parameter
         */
        string,
        /**
         * Object as parameter
         */
        object
    };

    /**
     * @brief Parameter can assume the following types: unsigned, int, double, bool, std::string
     *
     * This class is used for parsing algorithms parameters from files and construct
     * parameters dictionaries for algorithms initialization.
     */
    class parameter
    {

    private:
        /**
         * Union of the avaliable parameter values
         */
        union param_value
        {
            /**
             * Unsigned parameter
             */
            unsigned u_number;
            /**
             * Signed parameter
             */
            int number;
            /**
             * Float parameter
             */
            double number_float;
            /**
             * Boolean parameter
             */
            bool boolean;
            /**
             * String parameter
             */
            std::string *string;

            param_value();

            param_value(unsigned u_number);

            param_value(int number);

            param_value(double number_float);

            param_value(bool boolean);

            param_value(const std::string &s);

            param_value(const char *s);

            param_value(value_t t);

            void destroy(value_t t);

            ~param_value();
        };

    private:
        param_value value_;
        value_t type_;

    public:
        /**
         * Constructor
         */
        parameter();
        /**
         * Constructor
         */
        parameter(const param_value &value, value_t type);
        /**
         * Constructor
         */
        parameter(const parameter &other);
        /**
         * Constructor
         */
        parameter(int num);
        /**
         * Constructor
         */
        parameter(unsigned u_num);
        /**
         * Constructor
         */
        parameter(double f_num);
        /**
         * Constructor
         */
        parameter(bool b);
        /**
         * Constructor
         */
        parameter(const std::string &s);
        /**
         * Constructor
         */
        parameter(const char *s);
        /**
         * Operator =
         */
        parameter operator=(const parameter &other);
        /**
         * Operator +
         */
        parameter operator+(const parameter &other);
        /**
         * Operator -
         */
        parameter operator-(const parameter &other);
        /**
         * Operator *
         */
        parameter operator*(const parameter &other);
        /**
         * Operator +=
         */
        parameter operator+=(const parameter &other);
        /**
         * Operator -=
         */
        parameter operator-=(const parameter &other);
        /**
         * Operator *=
         */
        parameter operator*=(const parameter &other);
        /**
         * Convert to unsigned
         */
        operator unsigned() const;
        /**
         * Convert to double
         */
        operator double() const;
        /**
         * Convert to bool
         */
        operator bool() const;
        /**
         * Convert to string
         */
        operator std::string() const;
        /**
         * Get parameter type
         */
        value_t type() const;
        /**
         * Destructor
         */
        ~parameter();

        /**
         * Output operator
         */
        friend std::ostream &operator<<(std::ostream &out, const parameter &param);
    };

    /**
     * Output operator
     */
    std::ostream &operator<<(std::ostream &out, const parameter &param);
}
