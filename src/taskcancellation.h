// Copyright (c) 2019 Bitcoin Association.
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#ifndef BITCOIN_TASKCANCELLATION_H
#define BITCOIN_TASKCANCELLATION_H

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace task
{
    class CCancellationSource;

    /**
     * An immutable token that can be provided to a long running task which can
     * periodically check whether it should cancel before completion.
     * Cancellation is triggered from the attached CCancellationSource(s).
     *
     * Tokens can be joined together from different sources so that if any of
     * the sources trigger cancellation the token is also canceled. Since tokens
     * are immutable `JoinToken()` returns a new token that is attached to all
     * the sources to which the source tokens are attached - this makes tokens
     * thread safe.
     */
    class CCancellationToken
    {
    public:
        CCancellationToken(std::shared_ptr<CCancellationSource> source)
            : mSource{std::move(source)}
        {/**/}

        bool IsCanceled() const
        {
            return
                std::any_of(
                    mSource.begin(),
                    mSource.end(),
                    [](auto source){ return source->IsCanceled(); });
        }

        static CCancellationToken JoinToken(
            const CCancellationToken& token1,
            const CCancellationToken& token2)
        {
            CCancellationToken newToken;
            // we don't care if some of the sources are duplicates as we don't
            // expect a large amount of sources and even less duplicates
            newToken.mSource.reserve(
                token1.mSource.size() + token2.mSource.size());
            newToken.mSource.insert(
                newToken.mSource.end(),
                token1.mSource.begin(),
                token1.mSource.end());
            newToken.mSource.insert(
                newToken.mSource.end(),
                token2.mSource.begin(),
                token2.mSource.end());

            return newToken;
        }

    private:
        CCancellationToken() = default;
        std::vector<std::shared_ptr<CCancellationSource>> mSource;
    };

    /**
     * A long running task cancellation source which is kept on the caller side
     * while the associated token is provided to the task to periodically check
     * whether it should terminate before completion.
     *
     * To create a new cancellation source static member function Make() should
     * be called.
     */
    class CCancellationSource : public std::enable_shared_from_this<CCancellationSource>
    {
    public:
        static std::shared_ptr<CCancellationSource> Make()
        {
            return
                std::shared_ptr<CCancellationSource>{new CCancellationSource{}};
        }

        CCancellationToken GetToken()
        {
            return CCancellationToken{shared_from_this()};
        }

        void Cancel() { mCanceled = true; }
        bool IsCanceled() const { return mCanceled; }

    private:
        CCancellationSource() = default;
        std::atomic<bool> mCanceled = false;
    };
}

#endif // BITCOIN_TASKCANCELLATION_H