#include <eosiolib/eosio.hpp>
#include <eosiolib/types.h>

using std::vector;
using byte = uint8_t;
using bytes = vector<byte>;

uint8_t _getNthNibbleOfBytes(uint n, bytes str) {
    return byte(n%2==0 ? uint8_t(str[n/2])/0x10 : uint8_t(str[n/2])%0x10);
}

void _getNibbleArray(bytes b, bytes *nibbles) {

    if (b.size() > 0) {
        uint8_t offset;
        uint nibbles_size;
        uint8_t hpNibble = uint8_t(_getNthNibbleOfBytes(0,b));

        if (hpNibble == 1 || hpNibble == 3) {
            nibbles_size = b.size() * 2 - 1;
            byte oddNibble = _getNthNibbleOfBytes(1,b);
            (*nibbles).push_back(oddNibble);
            offset = 1;
        } else {

            // TAL - addition, recheck
            if (b.size() == 1) {
                (*nibbles).push_back(_getNthNibbleOfBytes(0,b));
                (*nibbles).push_back(_getNthNibbleOfBytes(1,b));
                return;
            }

            nibbles_size= (b.size() * 2- 2);
            offset = 0;
        }

        for (uint i = offset; i < nibbles_size; i++) {
            (*nibbles).push_back(_getNthNibbleOfBytes(i-offset+2,b));
        }
    }

    return;
}

uint _nibblesToTraverse(bytes encodedPartialPath, bytes path, uint pathPtr) {

    uint len;
    // encodedPartialPath has elements that are each two hex characters (1 byte), but partialPath
    // and slicedPath have elements that are each one hex character (1 nibble)
    bytes partialPath;
    _getNibbleArray(encodedPartialPath, &partialPath);

    bytes slicedPath;
    slicedPath.resize(partialPath.size());
    memcpy(&slicedPath[0], &partialPath[0], partialPath.size());

    // pathPtr counts nibbles in path
    // partialPath.length is a number of nibbles
    for (uint i=pathPtr; i<pathPtr+partialPath.size(); i++) {
        byte pathNibble = path[i];
        slicedPath[i-pathPtr] = pathNibble;
    }

    //if (keccak256(partialPath) == keccak256(slicedPath)) {
    if (memcmp(&partialPath[0], &slicedPath[0], partialPath.size()) == 0) {
        len = partialPath.size();
    } else {
        len = 0;
    }
    return len;
}

// TODO - get values as reference/pointers
int trieValue(bytes encodedPath,
              bytes value,
              bytes all_parent_nodes_rlps,
              vector<uint> all_parnet_rlp_sizes,
              uint8_t *root) {

    vector<bytes> parent_nodes_rlps;
    uint offset = 0;
    // TODO: switch to vectors
    for(uint i=0; i< all_parnet_rlp_sizes.size(); i++) {
        bytes::const_iterator first = all_parent_nodes_rlps.begin() + offset;
        bytes::const_iterator last = first + all_parnet_rlp_sizes[i];

        bytes newVec(first, last);
        parent_nodes_rlps.push_back(newVec);
        offset += all_parnet_rlp_sizes[i];
    }

    uint8_t *nodeKey = root;
    uint pathPtr = 0;

    bytes path;
    _getNibbleArray(encodedPath, &path);
    if (path.size() == 0) {return false;}

    for (uint i = 0; i < parent_nodes_rlps.size(); i++) {
        if (pathPtr > path.size()) {return false;}

        bytes *currentNodePtr = &parent_nodes_rlps[i];

        uint8_t hash[32];
        keccak256(hash, &(*currentNodePtr)[0], (*currentNodePtr).size());

        if (memcmp(nodeKey, hash, 32) != 0) {
            printf("wrong 1!\n");
          return false;
        }

        rlp_item currentNodeList[17];
        uint current_node_list_len;
        decode_list(&(*currentNodePtr)[0], currentNodeList, &current_node_list_len);

        if (current_node_list_len == 17) {
            printf("branch node\n");
            if (pathPtr == path.size()) {

                uint8_t hash1[32];
                keccak256(hash1, currentNodeList[16].content, currentNodeList[16].len);

                uint8_t hash2[32];
                keccak256(hash2, &value[0], value.size());

                if (memcmp(hash1, hash2, 32) == 0) {
                    printf("right 2!\n");
                    return true;
                } else {
                    printf("wrong 2!\n");
                    return false;
                }
            }

            uint nextPathNibble = uint(path[pathPtr]);
            if (nextPathNibble > 16) {return false;}
            nodeKey = currentNodeList[nextPathNibble].content;
            pathPtr += 1;
        } else if (current_node_list_len == 2) {

            // create bytes out of array
            bytes currentNodeList_0_vec;
            currentNodeList_0_vec.resize(currentNodeList[0].len);
            memcpy(&currentNodeList_0_vec[0], currentNodeList[0].content, currentNodeList[0].len);

            pathPtr += _nibblesToTraverse(currentNodeList_0_vec, path, pathPtr);

            if (pathPtr == path.size()) { //leaf node
                printf("leaf node\n");
                uint8_t hash1[32];
                keccak256(hash1, currentNodeList[1].content, currentNodeList[1].len);

                uint8_t hash2[32];
                keccak256(hash2, &value[0], value.size());

                if (memcmp(hash1, hash2, 32) == 0) {
                    printf("right 3!\n");
                    return true;
                } else {
                    printf("wrong 3!\n");
                    return false;
                }
            }

            printf("extension node\n");
            if (_nibblesToTraverse(currentNodeList_0_vec, path, pathPtr) == 0) {
                return false;
            }

            nodeKey = currentNodeList[1].content;
        } else {
          printf("wrong 4!\n");
          return false;
        }
    }

    return false;
}
