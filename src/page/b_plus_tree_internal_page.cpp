/**
 * b_plus_tree_internal_page.cpp
 */
#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "page/b_plus_tree_internal_page.h"

namespace cmudb {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id,
                                          page_id_t parent_id)
{
	// 8 byte aligned even though the header is 28 bytes
	const size_t header_size = 32;
	const size_t sz = PAGE_SIZE - header_size;
	const size_t elems = (sz / sizeof(MappingType));

	SetPageType(IndexPageType::INTERNAL_PAGE);
	SetPageId(page_id);
	SetParentPageId(parent_id);
	SetMaxSize(elems);
	SetSize(0);
	cached_sibling_idx_ = static_cast<uint32_t>(-1);
	cached_curr_idx_ = static_cast<uint32_t>(-1);
}

// during lookup, cached the idx of the key that was followed.
// useful during delete to find the sibling
INDEX_TEMPLATE_ARGUMENTS
uint32_t B_PLUS_TREE_INTERNAL_PAGE_TYPE::GetCachedSiblingIndex()
{
	return cached_sibling_idx_;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetCachedSiblingIndex(uint32_t idx)
{
	assert (idx == static_cast<uint32_t>(-1));
	cached_sibling_idx_ = idx;
}

INDEX_TEMPLATE_ARGUMENTS
uint32_t B_PLUS_TREE_INTERNAL_PAGE_TYPE::GetCachedCurrentIndex()
{
	return cached_curr_idx_;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetCachedCurrentIndex(uint32_t idx)
{
	assert (idx == static_cast<uint32_t>(-1));
	cached_curr_idx_ = idx;
}

/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
KeyType B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const {
  // replace with your own code
  KeyType key;

  if (index < 0 || index >= GetSize())
	  throw std::invalid_argument("index out of range");
  key = array[index].first;
  return key;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key)
{
	if (index < 0 || index >= GetSize())
		throw std::invalid_argument("index out of range");
	array[index].first = key;
}

INDEX_TEMPLATE_ARGUMENTS
page_id_t B_PLUS_TREE_INTERNAL_PAGE_TYPE::Convert(ValueType value)
{
	return static_cast<page_id_t>(value);
}

/*
 * Helper method to find and return array index(or offset), so that its value
 * equals to input "value"
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const {
	// TODO: why do we need this??

	for (int idx = 0; idx < GetSize(); idx++)
	{
		if (array[idx].second == value)
			return idx;
	}

	// idx not found
	assert (false);
}

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const
{
	if (index < 0 || index >= GetSize())
		throw std::invalid_argument("index out of range");
	return array[index].second;
}

INDEX_TEMPLATE_ARGUMENTS
const MappingType &B_PLUS_TREE_INTERNAL_PAGE_TYPE::GetItem(int index) {
  // replace with your own code
  if ((index < 0) || (index >= GetSize()))
	  throw std::invalid_argument("received invalid index");
  return array[index];
}
/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * Find and return the child pointer(page_id) which points to the child page
 * that contains input "key"
 * Start the search from the second key(the first key should always be invalid)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType
B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key,
	const KeyComparator &comparator)
{
	// lookup in internal node is a bit different.
	// we need to find the smallest key that is >= 'key'
	// the 0th index for key is invalid
	assert (GetSize() > 1);

	int low = 1;
	int high = GetSize() - 1;
	int mid = (low + ((high - low) / 2));
	ValueType val;

	while (low <= high)
	{
		if (comparator(key, array[mid].first) == 0)
		{
			val = array[mid].second;
			// there will be atleast one value to the left of this
			// idx
			assert((mid - 1) >= 0);
			cached_sibling_idx_ = mid - 1;
			cached_curr_idx_ = mid;
			return val;
		}
		else if (comparator(key, array[mid].first) < 0)
		{
			high = mid -1;
		}
		else if (comparator(key, array[mid].first) > 0)
		{
			low = mid + 1;
		}

		mid = (low + ((high - low) / 2));
	}

	// (low - 1) contains the appropriate value
	val = array[low - 1].second;
	assert ((low - 1) >= 0);
	if ((low - 1) == GetSize() - 1)
	{
		// if this is the last idx, then we use the left sibling
		assert ((low - 2) >= 0);
		cached_sibling_idx_ = low - 2;
	}
	else
	{
		// we use the right sibling by default
		assert (low < GetSize());
		cached_sibling_idx_ = low;
	}
	assert ((low - 1) >= 0);
	cached_curr_idx_ = low - 1;

	return val;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Populate new root page with old_value + new_key & new_value
 * When the insertion cause overflow from leaf page all the way upto the root
 * page, you should create a new root page and populate its elements.
 * NOTE: This method is only called within InsertIntoParent()(b_plus_tree.cpp)
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::PopulateNewRoot(
    const ValueType &old_value, const KeyType &new_key,
    const ValueType &new_value)
{
	// the page has to be new and also already initialized before
	// calling this function.
	assert (GetSize() == 0);
	// key at index 0 is invalid
	array[0].second = old_value;
	array[1].first = new_key;
	array[1].second = new_value;

	// update the size to 2
	SetSize(2);
}
/*
 * Insert new_key & new_value pair right after the pair with its value ==
 * old_value
 * @return:  new size after insertion
 */
INDEX_TEMPLATE_ARGUMENTS
int B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertNodeAfter(
    const ValueType &old_value, const KeyType &new_key,
    const ValueType &new_value) {

	// make sure we have enough space
	// although this insertion can cause a split

	assert (GetSize() < GetMaxSize());
	const int idx = ValueIndex(old_value);

	const int eltsToMove = (GetSize() - 1 - idx) * sizeof(MappingType);
	if (eltsToMove)
	{
		memmove(&array[idx+1], &array[idx+2], eltsToMove);
	}

	array[idx+1].first = new_key;
	array[idx+1].second = new_value;

	IncreaseSize(1);
	// returns 0 if there is no space and we need to split
    return (GetMaxSize() - GetSize()) * sizeof(MappingType);
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/*
 * Remove half of key & value pairs from this page to "recipient" page
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveHalfTo(
    BPlusTreeInternalPage *recipient,
    BufferPoolManager *buffer_pool_manager)
{
	assert (GetSize() > 1);
	const int mid = GetSize() / 2;
	const int high = GetSize();
	MappingType* const items = &array[mid];
	recipient->CopyHalfFrom(items, high - mid, buffer_pool_manager);

	SetSize(mid);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyHalfFrom(
    MappingType *items, int size, BufferPoolManager *buffer_pool_manager)
{
	// ensure that we don't cause an overflow
	const int currElements = GetSize();
	// make sure when we copy stuff from another page due to a  split
	// the split page should have 0 elements
	assert (currElements == 0);
	assert (currElements + size < GetMaxSize());
	MappingType* src = &array[currElements];
	for (int i = 0; i < size; i++)
	{
		src[i] = items[i];
	}

	IncreaseSize(size);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Remove the key & value pair in internal page according to input index(a.k.a
 * array offset)
 * NOTE: store key&value pair continuously after deletion
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Remove(int index)
{
	assert (GetSize() > 0);
	const int sz = (GetSize() - 1 - index) * sizeof(MappingType);
	assert (sz >= 0);

	if (sz)
		memmove(&array[index+1], &array[index], sz);
	IncreaseSize(-1);
}

/*
 * Remove the only key & value pair in internal page and return the value
 * NOTE: only call this method within AdjustRoot()(in b_plus_tree.cpp)
 */
INDEX_TEMPLATE_ARGUMENTS
ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveAndReturnOnlyChild() {
	assert (GetSize() == 1); // the key is dummy
	ValueType value = array[0].second;
	SetSize(0);

	return value;
}
/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all of key & value pairs from this page to "recipient" page, then
 * update relavent key & value pair in its parent page.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(
    BPlusTreeInternalPage *recipient, int index_in_parent,
    BufferPoolManager *buffer_pool_manager)
{
	const int elemCount = GetSize();

	assert (elemCount > 0);
	recipient->CopyAllFrom(array, elemCount, buffer_pool_manager);
	SetSize(0);
	// always merge with the left sibling
	// once the merge is done, we need to remove the key/value pair
	// from the parent
	// TODO
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyAllFrom(
    MappingType *items, int size, BufferPoolManager *buffer_pool_manager)
{
	// TODO: why do we need a buffer pool manager here??
	// make sure we don't overflow
	assert (GetSize() + size <= GetMaxSize());

	// ensure that we move only from the right sibling to the left
	// or else we would mess the order
	MappingType* const arr = &array[GetSize()];
	for (int idx = 0; idx < size; idx++)
	{
		arr[idx] = items[idx];
	}

	IncreaseSize(size);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to tail of "recipient"
 * page, then update relavent key & value pair in its parent page.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(
    BPlusTreeInternalPage *recipient,
    BufferPoolManager *buffer_pool_manager)
{
	// this should only be called when trying to move a k-v pair from
	// the right sibling to the left sibling
	assert(GetSize() > 0);
	const MappingType item = GetItem(0);

	recipient->CopyLastFrom(item, buffer_pool_manager);
	IncreaseSize(-1);

	// need to resize
	memmove(&array[0], &array[1], GetSize() * sizeof(MappingType));

}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyLastFrom(
    const MappingType &pair, BufferPoolManager *buffer_pool_manager)
{
	const int sz = GetSize();
	assert (sz < GetMaxSize());

	array[sz] = pair;
	IncreaseSize(1);
}

/*
 * Remove the last key & value pair from this page to head of "recipient"
 * page, then update relavent key & value pair in its parent page.
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(
    BPlusTreeInternalPage *recipient, int parent_index,
    BufferPoolManager *buffer_pool_manager)
{
	// this should only happen from the left sibling to the right sibling
	// to preserve order
	const int sz = GetSize();
	assert (sz > 0);

	auto elem = GetItem(sz - 1);
	recipient->CopyFirstFrom(elem, parent_index, buffer_pool_manager);
	IncreaseSize(-1);

	// TODO: update in the parent
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::CopyFirstFrom(
    const MappingType &pair, int parent_index,
    BufferPoolManager *buffer_pool_manager)
{
	const int sz = GetSize();
	assert ((sz > 1) && (sz < GetMaxSize()));

	memmove(&array[1], &array[0], sz * sizeof(MappingType));
	array[0] = pair;
	IncreaseSize(1);

	// TODO: update the parent index's key
}

/*****************************************************************************
 * DEBUG
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::QueueUpChildren(
    std::queue<BPlusTreePage *> *queue,
    BufferPoolManager *buffer_pool_manager) {
  for (int i = 0; i < GetSize(); i++) {
    auto *page = buffer_pool_manager->FetchPage(array[i].second);
    if (page == nullptr)
      throw Exception(EXCEPTION_TYPE_INDEX,
                      "all page are pinned while printing");
    BPlusTreePage *node =
        reinterpret_cast<BPlusTreePage *>(page->GetData());
    queue->push(node);
  }
}

INDEX_TEMPLATE_ARGUMENTS
std::string B_PLUS_TREE_INTERNAL_PAGE_TYPE::ToString(bool verbose) const {
  if (GetSize() == 0) {
    return "";
  }
  std::ostringstream os;
  if (verbose) {
    os << "[pageId: " << GetPageId() << " parentId: " << GetParentPageId()
       << "]<" << GetSize() << "> ";
  }

  int entry = verbose ? 0 : 1;
  int end = GetSize();
  bool first = true;
  while (entry < end) {
    if (first) {
      first = false;
    } else {
      os << " ";
    }
    os << std::dec << array[entry].first.ToString();
    if (verbose) {
      os << "(" << array[entry].second << ")";
    }
    ++entry;
  }
  return os.str();
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t,
                                           GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t,
                                           GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t,
                                           GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t,
                                           GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t,
                                           GenericComparator<64>>;
} // namespace cmudb
