#include "parameter.h"

namespace scheduling_problem
{
    /* param value */

    /**
     * @brief Default-constructs the storage for a parameter value.
     * @details Initializes the internal pointer variant to nullptr. The active value
     *          is determined by the accompanying value_t the owner (parameter) keeps.
     *          No heap is allocated here.
     */
    parameter::param_value::param_value() : string(nullptr) {}

    /**
     * @brief Constructs an unsigned integer value holder.
     * @param u_number Unsigned integer to store.
     */
    parameter::param_value::param_value(unsigned u_number) : u_number(u_number) {}

    /**
     * @brief Constructs a signed integer value holder.
     * @param number Signed integer to store.
     */
    parameter::param_value::param_value(int number) : number(number) {}

    /**
     * @brief Constructs a floating-point value holder.
     * @param number_float Double precision value to store.
     */
    parameter::param_value::param_value(double number_float) : number_float(number_float) {}

    /**
     * @brief Constructs a boolean value holder.
     * @param boolean Boolean value to store.
     */
    parameter::param_value::param_value(bool boolean) : boolean(boolean) {}

    /**
     * @brief Constructs a string value holder by copying the string.
     * @param s String to copy into heap-allocated storage.
     * @note Allocates on the heap and stores a pointer. Must be freed via destroy(value_t::string).
     */
    parameter::param_value::param_value(const std::string &s) : string(new std::string(s)) {}

    /**
     * @brief Constructs a string value holder from a C-string.
     * @param s Null-terminated C string to copy.
     */
    parameter::param_value::param_value(const char *s) : param_value(std::string(s)) {}

    /**
     * @brief Tagged constructor that zero-initializes the chosen alternative.
     * @param t Discriminant indicating which alternative to initialize.
     * @details For @c value_t::string allocates an empty string on the heap.
     *          For numeric/boolean alternatives sets zero-equivalent values.
     */
    parameter::param_value::param_value(value_t t) : string(nullptr)
    {
        switch (t)
        {
        case value_t::number_integer_unsigned:
            u_number = static_cast<unsigned>(0);
            break;
        case value_t::number_integer:
            number = 0;
            break;
        case value_t::number_float:
            number_float = 0.0;
            break;
        case value_t::boolean:
            boolean = false;
            break;
        case value_t::string:
            string = new std::string();
            break;
        default:
            break;
        }
    }

    /**
     * @brief Destroys heap-managed resources for the active alternative.
     * @param t Discriminant of the currently active alternative.
     * @note Only @c value_t::string requires explicit destruction (delete).
     *       Other alternatives are trivially destructible.
     */
    void parameter::param_value::destroy(value_t t)
    {
        switch (t)
        {
        case value_t::string:
            delete string;
            break;
        default:
            break;
        }
    }

    /**
     * @brief Trivial destructor.
     * @details Does not call destroy() by design; the owning @c parameter
     *          calls destroy() according to its current @c value_t.
     */
    parameter::param_value::~param_value()
    {
    }

    /* parameter */

    /**
     * @brief Default-constructs a parameter as an empty object.
     * @details Sets type to @c value_t::object and value storage to default.
     */
    parameter::parameter() : type_(value_t::object), value_() {}

    /**
     * @brief Constructs a parameter from a raw storage @c param_value and a type tag.
     * @param value Pre-filled storage to copy from (according to @p type).
     * @param type  Discriminant describing which alternative is active.
     * @note Performs deep copy for strings; shallow copy for trivially stored types.
     */
    parameter::parameter(const param_value &value, value_t type) : type_(type)
    {
        switch (type_)
        {
        case value_t::number_integer_unsigned:
            value_.u_number = value.u_number;
            break;
        case value_t::number_integer:
            value_.number = value.number;
            break;
        case value_t::number_float:
            value_.number_float = value.number_float;
            break;
        case value_t::boolean:
            value_.boolean = value.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*value.string);
            break;
        default:
            break;
        }
    }

    /**
     * @brief Copy-constructs a parameter.
     * @param other Source parameter to copy.
     * @details Copies type and value. Performs deep copy for strings.
     */
    parameter::parameter(const parameter &other)
    {
        type_ = other.type();
        switch (other.type())
        {
        case value_t::number_integer_unsigned:
            value_.u_number = other.value_.u_number;
            break;
        case value_t::number_integer:
            value_.number = other.value_.number;
            break;
        case value_t::number_float:
            value_.number_float = other.value_.number_float;
            break;
        case value_t::boolean:
            value_.boolean = other.value_.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*other.value_.string);
            break;
        default:
            break;
        }
    }

    /**
     * @brief Constructs a signed integer parameter.
     * @param num Value to store.
     */
    parameter::parameter(int num) : parameter(param_value(num), value_t::number_integer) {}

    /**
     * @brief Constructs an unsigned integer parameter.
     * @param u_num Value to store.
     */
    parameter::parameter(unsigned u_num) : parameter(param_value(u_num), value_t::number_integer_unsigned) {}

    /**
     * @brief Constructs a floating-point parameter.
     * @param f_num Value to store.
     */
    parameter::parameter(double f_num) : parameter(param_value(f_num), value_t::number_float) {}

    /**
     * @brief Constructs a boolean parameter.
     * @param b Value to store.
     */
    parameter::parameter(bool b) : parameter(param_value(b), value_t::boolean) {}

    /**
     * @brief Constructs a string parameter by copying.
     * @param s String to copy.
     */
    parameter::parameter(const std::string &s) : parameter(param_value(s), value_t::string) {}

    /**
     * @brief Constructs a string parameter from a C-string.
     * @param s Null-terminated string to copy.
     */
    parameter::parameter(const char *s) : parameter(param_value(s), value_t::string) {}

    /**
     * @brief Copy-assigns from another parameter.
     * @param other Source parameter to copy.
     * @return Reference to this parameter.
     * @details Destroys current heap-managed state if necessary, then deep/shallow copies
     *          the new value based on @p other.type().
     */
    parameter &parameter::operator=(const parameter &other)
    {
        if (this == &other)
            return *this;

        value_.destroy(type_);
        type_ = other.type();
        switch (other.type())
        {
        case value_t::number_integer_unsigned:
            value_.u_number = other.value_.u_number;
            break;
        case value_t::number_integer:
            value_.number = other.value_.number;
            break;
        case value_t::number_float:
            value_.number_float = other.value_.number_float;
            break;
        case value_t::boolean:
            value_.boolean = other.value_.boolean;
            break;
        case value_t::string:
            value_.string = new std::string(*other.value_.string);
            break;
        default:
            break;
        }
        return *this;
    }

    /**
     * @brief Adds two parameters of the same type.
     * @param other Right-hand operand; must have the same @c type() as @c *this.
     * @return Sum as a new parameter; returns a copy of @c *this if types differ.
     * @details Supports unsigned, integer, floating-point addition and string concatenation.
     */
    parameter parameter::operator+(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type())
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number += other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number += other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float += other.value_.number_float;
                break;
            case value_t::string:
                sum.value_.string->operator+=(*other.value_.string);
                break;
            default:
                break;
            }
        }
        return sum;
    }

    /**
     * @brief Subtracts two parameters of the same numeric type.
     * @param other Right-hand operand; must have the same @c type() and not be string.
     * @return Difference as a new parameter; returns a copy of @c *this if types differ.
     */
    parameter parameter::operator-(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type() && type() != value_t::string)
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number -= other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number -= other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float -= other.value_.number_float;
                break;
            default:
                break;
            }
        }
        return sum;
    }

    /**
     * @brief Multiplies two parameters of the same numeric type.
     * @param other Right-hand operand; must have the same @c type() and not be string.
     * @return Product as a new parameter; returns a copy of @c *this if types differ.
     */
    parameter parameter::operator*(const parameter &other)
    {
        parameter sum = *this;
        if (type() == other.type() && type() != value_t::string)
        {
            switch (other.type())
            {
            case value_t::number_integer_unsigned:
                sum.value_.u_number *= other.value_.u_number;
                break;
            case value_t::number_integer:
                sum.value_.number *= other.value_.number;
                break;
            case value_t::number_float:
                sum.value_.number_float *= other.value_.number_float;
                break;
            default:
                break;
            }
        }
        return sum;
    }

    /**
     * @brief Compound addition assignment.
     * @param other Right-hand operand.
     * @return Reference to this parameter after addition.
     */
    parameter &parameter::operator+=(const parameter &other)
    {
        *this = *this + other;
        return *this;
    }

    /**
     * @brief Compound subtraction assignment.
     * @param other Right-hand operand.
     * @return Reference to this parameter after subtraction.
     */
    parameter &parameter::operator-=(const parameter &other)
    {
        *this = *this - other;
        return *this;
    }

    /**
     * @brief Compound multiplication assignment.
     * @param other Right-hand operand.
     * @return Reference to this parameter after multiplication.
     */
    parameter &parameter::operator*=(const parameter &other)
    {
        *this = *this * other;
        return *this;
    }

    /**
     * @brief Converts the parameter to @c unsigned.
     * @return Unsigned value obtained from the current alternative.
     * @details For booleans returns 0 or 1; for strings returns 0.
     */
    parameter::operator unsigned() const
    {
        unsigned res(0);
        switch (type())
        {
        case value_t::number_integer:
            res = unsigned(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = unsigned(value_.u_number);
            break;
        case value_t::number_float:
            res = unsigned(value_.number_float);
            break;
        case value_t::boolean:
            res = unsigned(value_.boolean);
            break;
        default:
            break;
        }
        return res;
    }

    /**
     * @brief Converts the parameter to @c double.
     * @return Double value obtained from the current alternative.
     * @details For booleans returns 0.0 or 1.0; for strings returns 0.0.
     */
    parameter::operator double() const
    {
        double res(0);
        switch (type())
        {
        case value_t::number_integer:
            res = double(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = double(value_.u_number);
            break;
        case value_t::number_float:
            res = double(value_.number_float);
            break;
        case value_t::boolean:
            res = double(value_.boolean);
            break;
        default:
            break;
        }
        return res;
    }

    /**
     * @brief Converts the parameter to @c bool.
     * @return Boolean value obtained from the current alternative.
     * @details For numerics applies C++ truthiness (zero is false). For strings returns false.
     */
    parameter::operator bool() const
    {
        bool res(false);
        switch (type())
        {
        case value_t::number_integer:
            res = bool(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = bool(value_.u_number);
            break;
        case value_t::number_float:
            res = bool(value_.number_float);
            break;
        case value_t::boolean:
            res = value_.boolean;
            break;
        default:
            break;
        }
        return res;
    }

    /**
     * @brief Converts the parameter to @c std::string.
     * @return String representation of the current alternative.
     * @details For numerics uses @c std::to_string; for boolean uses "0"/"1";
     *          for strings returns the stored string; for object returns empty.
     */
    parameter::operator std::string() const
    {
        std::string res;
        switch (type())
        {
        case value_t::number_integer:
            res = std::to_string(value_.number);
            break;
        case value_t::number_integer_unsigned:
            res = std::to_string(value_.u_number);
            break;
        case value_t::number_float:
            res = std::to_string(value_.number_float);
            break;
        case value_t::boolean:
            res = std::to_string(value_.boolean);
            break;
        case value_t::string:
            res = *value_.string;
            break;
        default:
            break;
        }
        return res;
    }

    /**
     * @brief Returns the discriminant of the currently stored alternative.
     * @return Active @c value_t.
     */
    value_t parameter::type() const
    {
        return type_;
    }

    /**
     * @brief Destructor.
     * @details Calls @c destroy for the active alternative to free heap memory if needed.
     */
    parameter::~parameter()
    {
        value_.destroy(type_);
    }

    /**
     * @brief Streams the parameter to an output stream.
     * @param out Output stream.
     * @param param Parameter to print.
     * @return Reference to @p out.
     * @details Prints the raw stored value for numbers/boolean and the string content for @c value_t::string.
     *          For @c value_t::object prints an empty string.
     */
    std::ostream &operator<<(std::ostream &out, const parameter &param)
    {
        switch (param.type())
        {
        case value_t::number_integer_unsigned:
            out << param.value_.u_number;
            break;
        case value_t::number_integer:
            out << param.value_.number;
            break;
        case value_t::number_float:
            out << param.value_.number_float;
            break;
        case value_t::boolean:
            out << param.value_.boolean;
            break;
        case value_t::string:
            out << *param.value_.string;
            break;
        case value_t::object:
            out << "";
        default:
            break;
        }
        return out;
    }
}
