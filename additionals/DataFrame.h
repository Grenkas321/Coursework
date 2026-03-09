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

namespace scheduling_problem::additionals
{
    /**
     * @brief Compute the printable length of an object via operator<<.
     * @tparam T Streamable type.
     * @param obj Object to measure.
     * @return Number of characters produced when streamed.
     */
    template <class T>
    inline static size_t length(const T &obj)
    {
        std::stringstream s;
        s << obj;
        return s.str().size();
    }

    /**
     * @brief Lightweight, header-only tabular container for experiment logs.
     *
     * @tparam IndexT       Row index type.
     * @tparam ColNameT     Column name type.
     * @tparam ContentT     Cell value type.
     * @tparam autoindexing If true, sequential indices are generated automatically.
     */
    template <class IndexT = unsigned,
              class ColNameT = std::string,
              class ContentT = double,
              bool autoindexing = false>
    class DataFrame
    {
    public:
        /**
         * @brief Associative series used to represent rows/columns.
         *
         * @tparam KeyT Key type (row index or column name).
         * @tparam ValT Value type (cell content).
         */
        template <class KeyT, class ValT>
        class Series : public std::map<KeyT, ValT>
        {
        public:
            /**
             * @brief Default constructor.
             */
            Series() = default;

            /**
             * @brief Copy constructor.
             */
            Series(const Series<KeyT, ValT> &other) = default;

            /**
             * @brief Construct from initializer list of pairs.
             * @param other Key/value pairs to insert.
             */
            Series(const std::initializer_list<std::pair<KeyT, ValT>> &other) : std::map<KeyT, ValT>()
            {
                for (auto &pair : other)
                    this->operator[](pair.first) = pair.second;
            }

            /**
             * @brief Construct from parallel sequences of content and indices.
             * @tparam ContentSeq Sequence with operator[] and size().
             * @tparam IndexSeq   Sequence with operator[] and size().
             * @param content Values.
             * @param index   Keys aligned with @p content.
             */
            template <class ContentSeq, class IndexSeq>
            Series(const ContentSeq &content, const IndexSeq &index) : std::map<KeyT, ValT>()
            {
                for (unsigned i(0); i < content.size(); i++)
                    this->operator[](index[i]) = content[i];
            }

            /**
             * @brief Extract keys in order.
             * @return Vector of keys.
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
             * @brief Extract values in order.
             * @return Vector of values.
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
             * @brief Pretty-print series as two aligned columns.
             * @param strm   Output stream.
             * @param series Series to print.
             * @return Output stream.
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

        /** @brief Row type: maps column name -> value. */
        typedef Series<ColNameT, ContentT> Row;
        /** @brief Column type: maps row index -> value. */
        typedef Series<IndexT, ContentT> Column;

    private:
        std::map<ColNameT, Column> columns_;
        unsigned count_;

    public:
        /**
         * @brief Default constructor (empty frame).
         */
        DataFrame() : count_(0), columns_() {}

        /**
         * @brief Copy constructor.
         */
        DataFrame(const DataFrame<IndexT, ColNameT, ContentT, autoindexing> &other) = default;

        /**
         * @brief Construct with predefined set of columns.
         * @param columns Column names.
         */
        DataFrame(const std::vector<ColNameT> &columns)
            : DataFrame<IndexT, ColNameT, ContentT, autoindexing>()
        {
            for (auto &col : columns)
                columns_[col] = Column();
        }

        /**
         * @brief Construct from dictionary of columns and an explicit index.
         * @param dict  Column name -> vector of values.
         * @param index Row indices aligned with column vectors.
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
         * @brief Construct from dictionary of columns with auto-generated index.
         * @param dict Column name -> vector of values.
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
         * @brief Construct from a 2D matrix with explicit columns and index.
         * @param data    Rows x Cols matrix (data[col] addressed in constructFromMatrix).
         * @param columns Column names.
         * @param index   Row indices.
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
         * @brief Construct from a 2D matrix with auto-generated index.
         * @param data    Rows x Cols matrix.
         * @param columns Column names.
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
         * @brief Access column by name (creates if missing).
         * @param col Column name.
         * @return Reference to the column series.
         */
        Column &operator[](const ColNameT &col)
        {
            return columns_[col];
        }

        /**
         * @brief Append a row at the next auto index.
         * @param row Values in column order.
         * @return True on success (shape matches), false otherwise.
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
         * @brief Append a row at a specific index.
         * @param row   Values in column order.
         * @param index Target row index.
         * @return True on success, false otherwise.
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
         * @brief Append a row from a (index, row-series) pair.
         * @param row Pair of index and Row (column-name -> value).
         * @return True on success, false otherwise.
         */
        bool append(const std::pair<IndexT, Row> &row)
        {
            bool is_appended = shape().second == row.second.size();

            for (auto &cell : row.second)
                if (!columns_.count(cell.first))
                    is_appended = false;

            if (is_appended)
                for (auto &cell : row.second)
                    columns_[cell.first][row.first] = cell.second;

            return is_appended;
        }

        /**
         * @brief Append multiple rows given as a map index->Row.
         * @param rows Row map.
         * @return True if all rows were appended successfully.
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
         * @brief Append rows from another compatible DataFrame.
         * @param other Source frame (same columns).
         * @return True on success, false otherwise.
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
         * @brief Report (rows, cols).
         * @return Pair {rows, cols}.
         */
        std::pair<size_t, size_t> shape() const
        {
            if (columns_.empty())
                return {0, 0};
            return {columns_.begin()->second.size(), columns_.size()};
        }

        /**
         * @brief Access columns map.
         * @return Map of column name -> column series.
         */
        std::map<ColNameT, Column> columns() const
        {
            return columns_;
        }

        /**
         * @brief Get column names.
         * @return Vector of column names.
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
         * @brief Materialize rows as a map index -> Row.
         * @return Map of rows.
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
         * @brief Get row indices in ascending order.
         * @return Vector of indices.
         */
        std::vector<IndexT> index() const
        {
            std::vector<IndexT> indices;
            if (columns_.empty())
                return indices;
            std::transform(columns_.begin()->second.begin(),
                           columns_.begin()->second.end(),
                           std::back_inserter(indices),
                           [](auto cell)
                           { return cell.first; });
            return indices;
        }

        /**
         * @brief Export frame to CSV.
         * @param filepath Output path.
         * @param sep      Column separator (default ';').
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
         * @brief Concatenate multiple DataFrames vertically (same schema).
         * @param dataframes Frames to combine.
         * @return Combined frame.
         */
        static DataFrame<IndexT, ColNameT, ContentT, autoindexing>
        combine(const std::vector<DataFrame<IndexT, ColNameT, ContentT, autoindexing>> &dataframes)
        {
            if (dataframes.empty())
                return DataFrame();
            DataFrame result(dataframes[0].colsnames());
            for (auto &df : dataframes)
                result.append(df);
            return result;
        }

    private:
        /**
         * @brief Internal: construct columns from a dictionary and explicit index.
         * @param dict  Column name -> vector of values.
         * @param index Row indices.
         */
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

        /**
         * @brief Internal: construct from matrix with columns and index.
         * @param data    Matrix of values.
         * @param columns Column names.
         * @param index   Row indices.
         */
        void constructFromMatrix(const std::vector<std::vector<ContentT>> &data,
                                 const std::vector<ColNameT> &columns,
                                 const std::vector<IndexT> &index)
        {
            if (columns.size() != data.size())
                return;

            for (size_t data_id = 0; data_id < columns.size(); ++data_id)
            {
                columns_[columns[data_id]] = Column(data[data_id], index);
            }
        }

        /**
         * @brief Internal: verify autoindexing expectations.
         */
        void checkIndexing()
        {
            if (!autoindexing)
            {
            }
            if (typeid(unsigned) != typeid(IndexT))
            {
            }
        }

        /**
         * @brief Compute padding widths for pretty-printing.
         * @return Pair {index_padding, column_padding_map}.
         */
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

        /**
         * @brief Pretty-print the dataframe as an aligned table.
         * @param strm Output stream.
         * @param df   DataFrame to print.
         * @return Output stream.
         */
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
