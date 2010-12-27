#ifndef UOME_DATA_INDEXEDONDEMANDFILELOADER_HPP
#define UOME_DATA_INDEXEDONDEMANDFILELOADER_HPP

#include "indexloader.hpp"
#include "ondemandfileloader.hpp"

namespace uome {
namespace data {

template <
typename ValueType
>
class IndexedOnDemandFileLoader {

public:
    IndexedOnDemandFileLoader(const boost::filesystem::path& indexPath, const boost::filesystem::path& dataPath, typename OnDemandFileLoader<ValueType>::ReadCallback readCallback) :
            indexLoader_(indexPath), dataLoader_(dataPath, readCallback) {
    }

    boost::shared_ptr<ValueType> get(unsigned int index, unsigned int userData) {
        const IndexBlock indexBlock = indexLoader_.get(index);

        // e.g. static blocks containing no data use an offset of 0xFFFFFFFFu
        if (indexBlock.offset_ == 0xFFFFFFFFu) {
            //LOGARG_WARN(LOGTYPE_DATA, "Trying to read nonexistant entry %u", index);
            return dataLoader_.get(0, indexLoader_.get(0), userData);
         } else {
             return dataLoader_.get(index, indexBlock, userData);
        }
    }

private:
    IndexLoader indexLoader_;
    OnDemandFileLoader<ValueType> dataLoader_;

};

}
}


#endif