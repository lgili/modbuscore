/**
 * @file mb_txpool.c
 * @brief Implementation of transaction pool with freelist and diagnostics.
 */

#include <modbus/mb_txpool.h>
#include <string.h>

mb_err_t mb_txpool_init(mb_txpool_t *txpool,
                        void *storage,
                        size_t tx_size,
                        size_t tx_count)
{
    if (!txpool || !storage || tx_size == 0 || tx_count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    memset(txpool, 0, sizeof(*txpool));
    
    mb_err_t err = mb_mempool_init(&txpool->pool, storage, tx_size, tx_count);
    if (err != MB_OK) {
        return err;
    }
    
    txpool->stats.capacity = tx_count;
    txpool->stats.available = tx_count;
    txpool->stats.in_use = 0;
    txpool->stats.high_water = 0;
    txpool->stats.total_acquired = 0;
    txpool->stats.total_released = 0;
    txpool->stats.failed_acquires = 0;
    txpool->current_in_use = 0;
    
    return MB_OK;
}

void mb_txpool_reset(mb_txpool_t *txpool) {
    if (!txpool) {
        return;
    }
    
    mb_mempool_reset(&txpool->pool);
    
    txpool->stats.in_use = 0;
    txpool->stats.available = txpool->stats.capacity;
    txpool->current_in_use = 0;
    // Keep high_water and total counters for diagnostics
}

void *mb_txpool_acquire(mb_txpool_t *txpool) {
    if (!txpool) {
        return NULL;
    }
    
    void *tx = mb_mempool_acquire(&txpool->pool);
    
    if (tx) {
        txpool->stats.total_acquired++;
        txpool->current_in_use++;
        txpool->stats.in_use = txpool->current_in_use;
        
        if (txpool->current_in_use > txpool->stats.high_water) {
            txpool->stats.high_water = txpool->current_in_use;
        }
        
        txpool->stats.available = mb_mempool_free_count(&txpool->pool);
    } else {
        txpool->stats.failed_acquires++;
    }
    
    return tx;
}

mb_err_t mb_txpool_release(mb_txpool_t *txpool, void *tx) {
    if (!txpool || !tx) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    mb_err_t err = mb_mempool_release(&txpool->pool, tx);
    if (err != MB_OK) {
        return err;
    }
    
    txpool->stats.total_released++;
    
    if (txpool->current_in_use > 0) {
        txpool->current_in_use--;
    }
    
    txpool->stats.in_use = txpool->current_in_use;
    txpool->stats.available = mb_mempool_free_count(&txpool->pool);
    
    return MB_OK;
}

mb_err_t mb_txpool_get_stats(const mb_txpool_t *txpool, mb_txpool_stats_t *stats) {
    if (!txpool || !stats) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    *stats = txpool->stats;
    return MB_OK;
}

size_t mb_txpool_capacity(const mb_txpool_t *txpool) {
    return txpool ? txpool->stats.capacity : 0;
}

size_t mb_txpool_available(const mb_txpool_t *txpool) {
    return txpool ? mb_mempool_free_count(&txpool->pool) : 0;
}

size_t mb_txpool_in_use(const mb_txpool_t *txpool) {
    return txpool ? txpool->current_in_use : 0;
}

size_t mb_txpool_high_water(const mb_txpool_t *txpool) {
    return txpool ? txpool->stats.high_water : 0;
}

bool mb_txpool_is_empty(const mb_txpool_t *txpool) {
    return mb_txpool_available(txpool) == 0;
}

bool mb_txpool_has_leaks(const mb_txpool_t *txpool) {
    if (!txpool) {
        return false;
    }
    
    // Consider it a leak if all transactions are allocated
    // This is a heuristic - might be legitimate under heavy load
    return mb_txpool_available(txpool) == 0 && txpool->stats.capacity > 0;
}
