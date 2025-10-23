class MessagesController < ApplicationController
  def index
    scope = Message.order(created_at: :desc)

    if params[:sender].present?
      scope = scope.where(sender_tag: params[:sender])
    end

    if params[:q].present?
      q = "%#{params[:q]}%"
      scope = scope.where("message LIKE ? OR nonce LIKE ?", q, q)
    end

    @messages = scope.limit(500)
    @distinct_senders = Message.distinct.order(:sender_tag).pluck(:sender_tag)
  end

  def show
    @message = Message.find(params[:id])
  end
end
