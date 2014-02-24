#Coordinator

```scala
class CoordinatorActor(amqpConnection: ActorRef) extends Actor {
  import CoordinatorActor._

  // sends the messages out
  private val output = context.actorOf(Props[JabberActor])

  def receive = {
    case b @ Begin(_) =>
      val rsa = context.actorOf(Props(new RecogSessionActor(amqpConnection, output)), UUID.randomUUID().toString)
      rsa.forward(b)
    case SingleImage(id, image) =>
      sessionActorFor(id).tell(RecogSessionActor.Image(image), sender)
    case FrameChunk(id, chunk) =>
      sessionActorFor(id).tell(RecogSessionActor.Frame(chunk), sender)
    case GetSessions =>
      sender ! context.children.filter(output !=).map(_.path.name).toList
  }

  // finds an ``ActorRef`` for the given session.
  private def sessionActorFor(id: String): ActorSelection = context.actorSelection(id)


}
```

#RecogSession

```scala
private[core] class RecogSessionActor(amqpConnection: ActorRef, outputActor: ActorRef) extends
  Actor with FSM[RecogSessionActor.State, RecogSessionActor.Data] with
  AmqpOperations with RecogSessionActorFormats with ImageEncoding {
  import RecogSessionActor._
  import CoordinatorActor._
  import scala.concurrent.duration._
  import context.dispatcher

  // default timeout for all states
  val stateTimeout = 60.seconds
  val emptyBehaviour: StateFunction = { case _ => stay() }
  val amqp = ConnectionOwner.createChildActor(amqpConnection, Props(new RpcClient()))

  startWith(Idle, Empty)

  when(Idle, stateTimeout) {
    case Event(Begin(minCoins), _) =>
      sender ! self.path.name
      goto(Active) using Starting(minCoins)
  }

  when(Active, stateTimeout) {
    case Event(Frame(frame), Starting(minCoins)) =>
      // Frame with no decoder yet. We will be needing the H264DecoderContext.
      val decoder = new H264DecoderContext(countCoins(minCoins))
      decoder.decode(frame, true)
      stay() using Running(decoder)
    case Event(Frame(frame), Running(decoder)) if frame.length > 0 =>
      // Frame with existing decoder. Just decode. (Teehee--I said ``Just``.)
      decoder.decode(frame, true)
      stay()
    case Event(Frame(_), Running(decoder)) =>
      // Last frame
      decoder.close()
      goto(Completed)

    case Event(Image(image), Starting(minCoins)) =>
      // Image with no decoder yet. We will be needing the ChunkingDecoderContext.
      val decoder = new ChunkingDecoderContext(countCoins(minCoins))
      decoder.decode(image, true)
      stay() using Running(decoder)
    case Event(Image(image), Running(decoder)) if image.length > 0 =>
      // Image with existing decoder. Shut up and apply.
      decoder.decode(image, true)
      stay()
    case Event(Image(_), Running(decoder)) =>
      // Empty image (following the previous case)
      decoder.close()
      goto(Completed)
  }

  when(Aborted)(emptyBehaviour)
  when(Completed)(emptyBehaviour)

  whenUnhandled {
    case Event(StateTimeout, _) => goto(Aborted)
  }

  onTransition {
    case _ -> Aborted => context.stop(self)
    case _ -> Completed => context.stop(self)
  }

  initialize()

  override def postStop(): Unit = {
    context.stop(amqp)
  }

  def countCoins(minCoins: Int)(f: Array[Byte]): Unit =
    amqpAsk[CoinResponse](amqp)("cm.exchange", "cm.coins.key", mkImagePayload(f)) onSuccess {
      case res => if (res.coins.size >= minCoins) outputActor ! res
    }

}
```

#Core and Configuration

```scala
trait CoreConfiguration {

  def amqpConnectionFactory: ConnectionFactory

}

trait ConfigCoreConfiguration extends CoreConfiguration {
  def system: ActorSystem

  // connection factory
  lazy val amqpConnectionFactory = {
    val amqpHost = system.settings.config.getString("cm.amqp.host")
    val cf = new ConnectionFactory()
    cf.setHost(amqpHost)
    cf
  }

}

trait Core {
  this: CoreConfiguration =>

  // start the actor system
  implicit lazy val system = ActorSystem("recog")

  // create a "connection owner" actor, which will try and reconnect automatically if the connection ins lost
  lazy val amqpConnection = system.actorOf(Props(new ConnectionOwner(amqpConnectionFactory)))

  // create the coordinator actor
  lazy val coordinator = system.actorOf(Props(new CoordinatorActor(amqpConnection)), "coordinator")

}
```

#Shell

```scala
object Shell extends App with Core with ConfigCoreConfiguration {

  import CoordinatorActor._
  import Commands._
  import akka.actor.ActorDSL._
  import Utils._

  implicit val _ = actor(new Act {
    become {
      case x => println(">>> " + x)
    }
  })

  // main command loop
  @tailrec
  private def commandLoop(): Unit = {
    Console.readLine() match {
      case QuitCommand => return
      case BeginCommand(count) => coordinator ! Begin(count.toInt)
      case GetSessionsCommand => coordinator ! GetSessions
      case ImageCommand(id, fileName) => readAll(fileName)(coordinator ! SingleImage(id, _))
      case H264Command(id, fileName) => readChunks(fileName, 64)(coordinator ! FrameChunk(id, _))

      case _ => println("WTF??!!")
    }

    commandLoop()
  }

  // start processing the commands
  commandLoop()
  system.shutdown()

}
```

#API

```scala
trait BasicRecogService extends Directives {
  import scala.concurrent.duration._
  import akka.pattern.ask
  import CoordinatorActor._
  import RecogService._

  implicit val timeout = akka.util.Timeout(2.seconds)

  def normalRoute(coordinator: ActorRef)(implicit ec: ExecutionContext): routing.Route =
    path(Recog) {
      post {
        complete((coordinator ? Begin(1)).mapTo[String])
      }
    } ~
    path(Recog / Rest) { sessionId =>
      post {
        entity(as[Array[Byte]]) { entity =>
          coordinator ! SingleImage(sessionId, entity)
          complete("{}")
        }
      }
    }

}

trait StreamingRecogService extends Directives {
  this: Actor =>

  import CoordinatorActor._
  import RecogService._

  def chunkedRoute(coordinator: ActorRef): routing.Route = {
    def handleChunksWith(creator: => Actor): RequestContext => Unit = {
      val handler = context.actorOf(Props(creator))
      sender ! RegisterChunkHandler(handler)

      {_ => ()}
    }

    path(Recog / MJPEG / Rest) { sessionId =>
      post {
        handleChunksWith(new StreamingRecogServiceActor(coordinator, sessionId, SingleImage))
      }
    } ~
    path(Recog / H264 / Rest)  { sessionId =>
      post {
        handleChunksWith(new StreamingRecogServiceActor(coordinator, sessionId, FrameChunk))
      }
    }
  }

}

class RecogServiceActor(coordinator: ActorRef) extends Actor with BasicRecogService with StreamingRecogService {
  import context.dispatcher
  val normal = normalRoute(coordinator)
  val chunked = chunkedRoute(coordinator)

  def receive: Receive = {
    // clients get connected to self (singleton handler)
    case _: Http.Connected => sender ! Http.Register(self)
    // POST to /recog/...
    case request: HttpRequest => normal(RequestContext(request, sender, request.uri.path).withDefaultSender(sender))
    // stream begin to /recog/[h264|mjpeg]/:id
    case ChunkedRequestStart(request) => chunked(RequestContext(request, sender, request.uri.path).withDefaultSender(sender))
  }

}

class StreamingRecogServiceActor[A]
  (coordinator: ActorRef, sessionId: String, message: (String, Array[Byte]) => A) extends Actor {
  ...
}
```

#Rest

```scala
object Rest extends App with Core with ConfigCoreConfiguration with Api
```
