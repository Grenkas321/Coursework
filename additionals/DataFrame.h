#pragma once

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <fstream>
#include <exception>
#include <boost/range/combine.hpp>
#include <boost/format.hpp>

// namespace scheduling_problem::additionals::ExceptionMessages
//{
//     boost::format bad_index_size = boost::format("Length of values (%1%) does not match length of index (%2%)");
//     const char* bad_arrays_length = "All sequences must be of the same length";
//     const char* autonumb_is_off = "its impossible to calculate index because autoindexing is false";
//     const char* index_isnot_integer = "its impossible to calculate index because its not unsigned integer";
//     boost::format bad_shape = boost::format("Shape of passed values is (%1%, %2%), indices imply (%3%, %4%)");
//     boost::format bad_column = boost::format("DataFrame is't contain column %1%");
// }

namespace scheduling_problem::additionals
{
    template <class T>
    inline static size_t length(const T &obj)
    {
        std::stringstream s;
        s << obj;
        return s.str().size();
    }

    /**
     * \brief Stores experiment results, so that result logging was easier.
     */
    template <class IndexT = unsigned,
              class ColNameT = std::string,
              class ContentT = double,
              bool autoindexing = false>
    class DataFrame
    {
    public:
        /**
         * \brief Stores %series of %DataFrames
         */
        template <class KeyT, class ValT>
        class Series : public std::map<KeyT, ValT>
        {
        public:
            /**
             * Constructor
             */
            Series() = default;
            /**
             * Constructor
             */
            Series(const Series<KeyT, ValT> &other) = default;
            /**
             * Constructor
             */
            Series(const std::initializer_list<std::pair<KeyT, ValT>> &other) : std::map<KeyT, ValT>()
            {
                for (auto &pair : other)
                    this->operator[](pair.first) = pair.second;
            }
            /**
             * Template constructor
             */
            template <class ContentSeq, class IndexSeq>
            Series(const ContentSeq &content, const IndexSeq &index) : std::map<KeyT, ValT>()
            {
                for (unsigned i(0); i < content.size(); i++)
                    this->operator[](index[i]) = content[i];
            }

            /**
             * Return vector of indexes
             */
            std::vector<KeyT> index() const
            {
                std::vector<KeyT> indices;
                std::transform(this->begin(),
                               this->end(),
                               std::back_inserter(indices),
                               [](auto pair)
                               { return pair.first; });
                return indices;
            }
            /**
             * Return vector of values
             */
            std::vector<ValT> values() const
            {
                std::vector<ValT> vals;
                std::transform(this->begin(),
                               this->end(),
                               std::back_inserter(vals),
                               [](auto pair)
                               { return pair.second; });
                return vals;
            }

            /**
             * Output operator
             */
            friend std::ostream &operator<<(std::ostream &strm, const Series<KeyT, ValT> &series)
            {
                size_t index_padding(0), values_padding(0), len;
                for (auto &key : series.index())
                    if (index_padding < (len = length(key)))
                        index_padding = len;

                for (auto &val : series.values())
                    if (values_padding < (len = length(val)))
                        values_padding = len;

                for (auto &keyval : series)
                {
                    strm << std::setw(index_padding) << keyval.first
                         << "    " << std::setw(values_padding) << keyval.second << std::endl;
                }

                return strm;
            }
        };

        /**
         * Define row by name and content
         */
        typedef Series<ColNameT, ContentT> Row;
        /**
         * Define column by index and content
         */
        typedef Series<IndexT, ContentT> Column;

    private:
        std::map<ColNameT, Column> columns_;
        unsigned count_;

    public:
        /**
         * Class constructor
         */
        DataFrame() : count_(0), columns_() {}
        /**
         * Class constructor
         */
        DataFrame(const DataFrame<IndexT, ColNameT, ContentT, autoindexing> &other) = default;
        /**
         * Class constructor
         */
        DataFrame(const std::vector<ColNameT> &columns)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            for (auto &col : columns)
                columns_[col] = Column();
        }
        /**
         * Class constructor
         */
        DataFrame(const std::unordered_map<ColNameT, std::vector<ContentT>> &dict,
                  const std::vector<IndexT> &index)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            constructFromDict(dict, index);

            if (autoindexing)
                count_ = (IndexT)index.size();
        }
        /**
         * Class constructor
         */
        DataFrame(const std::unordered_map<ColNameT, std::vector<ContentT>> &dict)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            checkIndexing();

            std::vector<unsigned> index;
            for (; count_ < dict.begin()->second.size(); count_++)
                index.push_back(count_);

            constructFromDict(dict, index);
        }
        /**
         * Class constructor
         */
        DataFrame(const std::vector<std::vector<ContentT>> &data,
                  const std::vector<ColNameT> &columns,
                  const std::vector<IndexT> &index)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            constructFromMatrix(data, columns, index);

            if (autoindexing)
                count_ = (unsigned)index.size();
        }
        /**
         * Class constructor
         */
        DataFrame(const std::vector<std::vector<ContentT>> &data,
                  const std::vector<ColNameT> &columns)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            checkIndexing();

            std::vector<unsigned> index;
            for (; count_ < data.begin()->size(); count_++)
                index.push_back(count_);

            constructFromMatrix(data, columns, index);
        }
        /**
         * Return value by index
         */
        Column &operator[](const ColNameT &col)
        {
            return columns_[col];
        }
        /**
         * Add row to data frame
         */
        bool append(const std::vector<ContentT> &row)
        {
            checkIndexing();

            auto is_appended = append(row, count_);

            if (is_appended)
                count_++;

            return is_appended;
        }
        /**
         * Add row to data frame at given position
         */
        bool append(const std::vector<ContentT> &row, const IndexT &index)
        {
            bool is_appended = shape().second == row.size();

            if (is_appended)
            {
                unsigned row_id(0);
                for (auto &col : columns_)
                {
                    ColNameT col_name = col.first;
                    ContentT val = row[row_id++];
                    col.second[index] = val;
                }
            }

            return is_appended;
        }
        /**
         * Add row to data frame at given index
         */
        bool append(const std::pair<IndexT, Row> &row)
        {
            bool is_appended = shape().second == row.second.size();

            // check columns' names
            for (auto &cell : row.second)
                if (!columns_.count(cell.first))
                    is_appended = false;

            if (is_appended)
                for (auto &cell : row.second)
                    columns_[cell.first][row.first] = cell.second;

            return is_appended;
        }
        /**
         * Add multiple rows to data frame at given indexes
         */
        bool append(const std::map<IndexT, Row> &rows)
        {
            bool is_appended = true;

            for (auto &row : rows)
            {
                is_appended &= append(row);
            }

            return is_appended;
        }
        /**
         * Add multiple rows to data frame at given indexes, rows and columns
         */
        bool append(const DataFrame<IndexT, ColNameT, ContentT, autoindexing> &other)
        {
            bool is_appended = shape().second == other.shape().second;
            for (auto &col : other.columns_)
                is_appended &= columns_.count(col.first);

            if (is_appended)
                append(other.rows());

            return is_appended;
        }

        /**
         * Return dataframe shape
         */
        std::pair<size_t, size_t> shape() const
        {
            return {columns_.begin()->second.size(), columns_.size()};
        }

        /**
         * Return number of dataframe columns
         */
        std::map<ColNameT, Column> columns() const
        {
            return columns_;
        }

        /**
         * Retrun dataframe column names
         */
        std::vector<ColNameT> colsnames() const
        {
            std::vector<ColNameT> cols;
            std::transform(columns_.begin(),
                           columns_.end(),
                           std::back_inserter(cols),
                           [](auto &col)
                           { return col.first; });
            return cols;
        }

        /**
         * Return dataframe rows
         */
        std::map<IndexT, Row> rows() const
        {
            std::map<IndexT, Row> df_rows;
            for (auto &col : columns_)
            {
                for (auto &cell : col.second)
                {
                    if (!df_rows.count(cell.first))
                        df_rows[cell.first] = Row();
                    df_rows[cell.first][col.first] = cell.second;
                }
            }

            return df_rows;
        }

        /**
         * Return vector of column indexes
         */
        std::vector<IndexT> index() const
        {
            std::vector<IndexT> indices;
            std::transform(columns_.begin()->second.begin(),
                           columns_.begin()->second.end(),
                           std::back_inserter(indices),
                           [](auto cell)
                           { return cell.first; });
            return indices;
        }

        /**
         * Convert dataframe to csv format
         */
        void toCsv(std::string filepath, char sep = ';') const
        {
            std::ofstream file;
            file.open(filepath);
            for (auto &col : columns_)
            {
                file << sep << col.first;
            }
            file << std::endl;
            for (auto &row : rows())
            {
                file << row.first;
                for (auto &cell : row.second)
                {
                    file << sep << cell.second;
                }
                file << std::endl;
            }
            file.close();
        }

        /**
         * Frame that is used to store experiment results, so that result logging was easier
         */
        static DataFrame<IndexT, ColNameT, ContentT, autoindexing>
        combine(const std::vector<DataFrame<IndexT, ColNameT, ContentT, autoindexing>> &dataframes)
        {
            DataFrame result(dataframes[0].colsnames());
            for (auto &df : dataframes)
                result.append(df);
            return result;
        }

    private:
        void constructFromDict(const std::unordered_map<ColNameT, std::vector<ContentT>> &dict,
                               const std::vector<IndexT> &index)
        {
            for (auto &col : dict)
            {
                if (col.second.size() != index.size())
                {
                }

                columns_[col.first] = Column(col.second, index);
            }
        }

        void constructFromMatrix(const std::vector<std::vector<ContentT>> &data,
                                 const std::vector<ColNameT> &columns,
                                 const std::vector<IndexT> &index)
        {
            unsigned data_id(0);
            for (auto &col : columns_)
            {
                columns_.second = Column(data[data_id++], index);
            }
        }

        void checkIndexing()
        {
            if (!autoindexing)
            {
            }
            if (typeid(unsigned) != typeid(IndexT))
            {
            }
        }

        std::pair<size_t, std::map<ColNameT, size_t>> calculatePadding()
        {
            size_t index_padding(0), len(0);
            for (auto &index : index())
                if ((len = length(index)) > index_padding)
                    index_padding = len;

            std::map<ColNameT, size_t> col_padding;
            for (auto &col : columns())
            {
                col_padding[col.first] = length(col.first);
                for (auto &cell : col.second)
                    if ((len = length(cell.second)) > col_padding[col.first])
                        col_padding[col.first] = len;
            }

            return {index_padding, col_padding};
        }

        friend std::ostream &operator<<(std::ostream &strm, DataFrame<IndexT, ColNameT, ContentT, autoindexing> &df)
        {
            auto padding = df.calculatePadding();
            auto index_padding(padding.first);
            auto col_padding(padding.second);

            strm << std::setw(index_padding) << "";
            for (auto &col : df.columns())
            {
                strm << " " << std::setw(col_padding[col.first]) << col.first;
            }

            strm << std::endl;
            for (auto row : df.rows())
            {
                strm << std::setw(index_padding) << row.first << " ";
                for (auto cell : row.second)
                    strm << std::setw(col_padding[cell.first]) << cell.second << " ";
                strm << std::endl;
            }
            return strm;
        }
    };
}
