#pragma once
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace Async {
// Results return to the owner's thread. Destroying the owner disconnects delivery.
template<class Work, class Done> void run(QObject *owner, Work work, Done done, QThreadPool *pool = QThreadPool::globalInstance()) {
    using Result = decltype(work());
    auto *watcher = new QFutureWatcher<Result>(owner);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, owner,
        [watcher, done = std::move(done)]() mutable {
            const auto result = watcher->result();
            watcher->deleteLater();
            done(result);
        });
    watcher->setFuture(QtConcurrent::run(pool, std::move(work)));
}
}
